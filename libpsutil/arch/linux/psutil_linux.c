/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <utmp.h>
#include <sched.h>
#include <ifaddrs.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <inttypes.h>
#include <sys/sysmacros.h>
#include <linux/ioprio.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include "../../psutil.h"
#include "psutil_linux.h"

// Forward declarations for socket-inode helpers used by
// psutil_linux_process_get_net_connections
static int collect_socket_inodes(pid_t pid, unsigned long** out,
                                 int** out_fds);
/* Defined further down; declared here because the per-process connection
 * getter runs before it. Without this the file compiled with an implicit
 * int return, which truncated the pointer on 64-bit builds. */
static psutil_net_connection* psutil_linux_collect_net(const char* kind,
                                                      const unsigned long* filter,
                                                      const int* filter_fds,
                                                      int nfilter,
                                                      int* count);
static int psutil_linux_read_net_file(const char* path, int family, int type,
                                      psutil_net_connection** conns,
                                      int* capacity, int* index,
                                      const unsigned long* filter,
                                      const int* filter_fds,
                                      int nfilter);

// Initialize the library
int psutil_linux_init(void) {
    // Linux doesn't need special initialization
    return 0;
}

// ====================================================================
// --- Internal helpers
// ====================================================================

// Read a whole file into memory, growing the buffer as needed. Embedded
// NUL bytes are preserved (needed for /proc/<pid>/cmdline). The returned
// buffer is always NUL terminated. NULL on error.
static char* read_file_all(const char* path, size_t* out_len) {
    FILE* fp = fopen(path, "rb");
    size_t cap = 4096;
    size_t len = 0;
    char* buf;

    if (fp == NULL) {
        return NULL;
    }
    buf = (char*)malloc(cap);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    for (;;) {
        size_t n;
        if (len + 1 >= cap) {
            char* nb = (char*)realloc(buf, cap * 2);
            if (nb == NULL) {
                free(buf);
                fclose(fp);
                return NULL;
            }
            buf = nb;
            cap *= 2;
        }
        n = fread(buf + len, 1, cap - len - 1, fp);
        len += n;
        if (n == 0) {
            break;
        }
    }
    buf[len] = '\0';
    fclose(fp);
    if (out_len != NULL) {
        *out_len = len;
    }
    return buf;
}

static double psutil_monotonic_now(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
    }
#endif
    return (double)time(NULL);
}

// _SC_CLK_TCK is not always 100 (some ARM / embedded kernels differ),
// so never hard-code it.
static long psutil_linux_clk_tck(void) {
    static long tck = 0;
    if (tck <= 0) {
        tck = sysconf(_SC_CLK_TCK);
        if (tck <= 0) {
            tck = 100;
        }
    }
    return tck;
}

// Boot time from /proc/stat "btime": second granularity, but immune to
// wall-clock jumps (unlike time(NULL) - uptime). Cached for the process
// lifetime; the value cannot change while the machine is up.
static double psutil_linux_boot_time_cached(void) {
    static double cached = 0.0;
    if (cached > 0.0) {
        return cached;
    }
    {
        FILE* fp = fopen("/proc/stat", "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (strncmp(line, "btime ", 6) == 0) {
                    long long btime = 0;
                    if (sscanf(line + 6, "%lld", &btime) == 1 && btime > 0) {
                        cached = (double)btime;
                    }
                    break;
                }
            }
            fclose(fp);
        }
    }
    if (cached <= 0.0) {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            cached = (double)time(NULL) - (double)si.uptime;
        }
    }
    return cached;
}

// Result of parsing /proc/<pid>/stat. One parse serves ppid, status,
// create_time, cpu_times and cpu_num instead of re-opening the file five
// times per process.
typedef struct {
    int valid;
    double stamp;               // psutil_monotonic_now() of the parse
    char state;
    psutil_pid_t ppid;
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long cutime;
    unsigned long long cstime;
    unsigned long long starttime;
    unsigned long num_threads;
    int cpu_num;
    char terminal[64];          // cached controlling tty path ("" = none)
    int terminal_valid;
} psutil_linux_stat;

#define PSUTIL_LINUX_STAT_TTL 0.001  // seconds the parse stays fresh

// Split the tail of /proc/<pid>/stat (everything after the last ')') into
// whitespace separated fields. Field numbering is relative to ')':
//   1=state  2=ppid  12=utime  13=stime  14=cutime  15=cstime
//   18=num_threads  20=starttime  38=processor
// Using a tokenizer instead of a long scanf keeps this independent from
// hardware-dependent field widths.
static int psutil_linux_parse_stat(const char* tail, psutil_linux_stat* st) {
    int field = 0;
    const char* s = tail;

    while (*s != '\0') {
        const char* end;
        size_t n;
        char buf[64];

        while (*s == ' ') {
            s++;
        }
        if (*s == '\0' || *s == '\n') {
            break;
        }
        end = s;
        while (*end != '\0' && *end != ' ' && *end != '\n') {
            end++;
        }
        n = (size_t)(end - s);
        if (n >= sizeof(buf)) {
            n = sizeof(buf) - 1;
        }
        memcpy(buf, s, n);
        buf[n] = '\0';
        field++;

        switch (field) {
            case 1:  st->state = buf[0]; break;
            case 2:  st->ppid = (psutil_pid_t)strtol(buf, NULL, 10); break;
            case 12: st->utime = strtoull(buf, NULL, 10); break;
            case 13: st->stime = strtoull(buf, NULL, 10); break;
            case 14: st->cutime = strtoull(buf, NULL, 10); break;
            case 15: st->cstime = strtoull(buf, NULL, 10); break;
            case 18: st->num_threads = strtoul(buf, NULL, 10); break;
            case 20: st->starttime = strtoull(buf, NULL, 10); break;
            case 38: st->cpu_num = (int)strtol(buf, NULL, 10); break;
            default: break;
        }
        s = end;
    }
    return field >= 20;
}

// Backend-private per-Process state lives in proc->_platform so the public
// struct stays backend agnostic.
static psutil_linux_stat* psutil_linux_stat_get(Process* proc, int force) {
    psutil_linux_stat* st;

    if (proc == NULL) {
        return NULL;
    }
    st = (psutil_linux_stat*)proc->_platform;
    if (st == NULL) {
        st = (psutil_linux_stat*)calloc(1, sizeof(*st));
        if (st == NULL) {
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
        proc->_platform = st;
    }

    if (!force && st->valid &&
        (psutil_monotonic_now() - st->stamp) < PSUTIL_LINUX_STAT_TTL) {
        return st;
    }

    st->valid = 0;
    {
        char path[64];
        char* data;
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)proc->pid);
        data = read_file_all(path, NULL);
        if (data == NULL) {
            psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
            return st;
        }
        {
            char* close_paren = strrchr(data, ')');
            if (close_paren != NULL &&
                psutil_linux_parse_stat(close_paren + 1, st)) {
                st->valid = 1;
                st->stamp = psutil_monotonic_now();
            } else {
                psutil_set_last_error(PSUTIL_ERROR_IO);
            }
        }
        free(data);
    }
    return st;
}

// Process functions
int psutil_linux_pid_exists(psutil_pid_t pid) {
    char path[64];
    struct stat st;

    snprintf(path, sizeof(path), "/proc/%d", (int)pid);
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

psutil_pid_t* psutil_linux_pids(int *count) {
    DIR *dir;
    struct dirent *entry;
    psutil_pid_t *pids = NULL;
    int size = 1024;
    int index = 0;

    *count = 0;
    pids = (psutil_pid_t*)malloc(size * sizeof(psutil_pid_t));
    if (pids == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }

    dir = opendir("/proc");
    if (dir == NULL) {
        free(pids);
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        char* endp = NULL;
        long pid = strtol(entry->d_name, &endp, 10);

        // Only accept pure numeric names; "self", "net" and friends are
        // silently skipped instead of being turned into pid 0 by atoi().
        if (pid > 0 && endp != NULL && *endp == '\0') {
            if (index >= size) {
                psutil_pid_t *new_pids;
                size *= 2;
                new_pids = (psutil_pid_t*)realloc(pids, (size_t)size * sizeof(psutil_pid_t));
                if (new_pids == NULL) {
                    free(pids);
                    closedir(dir);
                    psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                    return NULL;
                }
                pids = new_pids;
            }
            pids[index++] = (psutil_pid_t)pid;
        }
    }

    closedir(dir);
    *count = index;
    return pids;
}

Process* psutil_linux_process_new(psutil_pid_t pid) {
    Process* proc;
    char path[64];
    struct stat st;

    // If pid is 0, use current process PID
    if (pid == 0) {
        pid = (psutil_pid_t)getpid();
    }

    // The old code handed back an object for any pid, so process_new(99999)
    // "succeeded" and every later getter failed silently.
    snprintf(path, sizeof(path), "/proc/%d", (int)pid);
    if (stat(path, &st) != 0) {
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return NULL;
    }

    proc = (Process*)malloc(sizeof(Process));
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }

    proc->pid = pid;
    proc->name = NULL;
    proc->exe = NULL;
    proc->cwd = NULL;
    proc->username = NULL;
    proc->create_time = 0.0;
    proc->ppid = 0;
    proc->status = PSUTIL_STATUS_RUNNING;
    proc->_platform = NULL;

    return proc;
}

void psutil_linux_process_free(Process* proc) {
    if (proc == NULL) {
        return;
    }

    if (proc->name != NULL) {
        free(proc->name);
    }
    if (proc->exe != NULL) {
        free(proc->exe);
    }
    if (proc->cwd != NULL) {
        free(proc->cwd);
    }
    if (proc->username != NULL) {
        free(proc->username);
    }
    if (proc->_platform != NULL) {
        free(proc->_platform);
    }

    free(proc);
}

psutil_pid_t psutil_linux_process_get_ppid(Process* proc) {
    psutil_linux_stat* st = psutil_linux_stat_get(proc, 0);
    if (st == NULL || !st->valid) {
        return 0;
    }
    return st->ppid;
}

const char* psutil_linux_process_get_name(Process* proc) {
    char path[64];

    if (proc == NULL) {
        return NULL;
    }
    if (proc->name != NULL) {
        return proc->name;
    }

    snprintf(path, sizeof(path), "/proc/%d/comm", (int)proc->pid);
    {
        FILE* fp = fopen(path, "r");
        if (fp != NULL) {
            char name[256];
            if (fgets(name, sizeof(name), fp) != NULL) {
                name[strcspn(name, "\n")] = '\0';
                proc->name = strdup(name);
            }
            fclose(fp);
        }
    }
    return proc->name;
}

const char* psutil_linux_process_get_exe(Process* proc) {
    char path[64];

    if (proc == NULL) {
        return NULL;
    }
    if (proc->exe != NULL) {
        return proc->exe;
    }

    snprintf(path, sizeof(path), "/proc/%d/exe", (int)proc->pid);
    {
        // PATH_MAX is 4096; 256 silently truncated deep paths (containers,
        // overlay mounts).
        char exe[4096];
        ssize_t len = readlink(path, exe, sizeof(exe) - 1);
        if (len > 0) {
            exe[len] = '\0';
            proc->exe = strdup(exe);
        } else {
            psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        }
    }
    return proc->exe;
}

char** psutil_linux_process_get_cmdline(Process* proc, int* count) {
    char path[64];
    size_t len = 0;
    char* data;
    char** argv = NULL;
    int argc = 0;
    size_t i;

    *count = 0;
    if (proc == NULL) {
        return NULL;
    }

    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)proc->pid);
    data = read_file_all(path, &len);
    if (data == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return NULL;
    }
    if (len == 0) {
        // Kernel threads have an empty cmdline; this is not an error.
        free(data);
        return NULL;
    }

    // Count non-empty NUL separated arguments. Reading is bounded by len,
    // so no out-of-bounds walk past the end of the buffer.
    i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && data[i] != '\0') {
            i++;
        }
        if (i > start) {
            argc++;
        }
        if (i < len) {
            i++;  // skip the separator
        }
    }

    argv = (char**)malloc(((size_t)argc + 1) * sizeof(char*));
    if (argv == NULL) {
        free(data);
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }

    i = 0;
    argc = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && data[i] != '\0') {
            i++;
        }
        if (i > start) {
            argv[argc] = (char*)malloc(i - start + 1);
            if (argv[argc] == NULL) {
                int j;
                for (j = 0; j < argc; j++) {
                    free(argv[j]);
                }
                free(argv);
                free(data);
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            memcpy(argv[argc], data + start, i - start);
            argv[argc][i - start] = '\0';
            argc++;
        }
        if (i < len) {
            i++;
        }
    }
    argv[argc] = NULL;

    free(data);
    *count = argc;
    return argv;
}

int psutil_linux_process_get_status(Process* proc) {
    psutil_linux_stat* st = psutil_linux_stat_get(proc, 0);
    if (st == NULL || !st->valid) {
        // The process disappeared or /proc/<pid>/stat is unreadable:
        // reporting RUNNING here would be a lie.
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return PSUTIL_STATUS_DEAD;
    }
    switch (st->state) {
        case 'R': return PSUTIL_STATUS_RUNNING;
        case 'S': return PSUTIL_STATUS_SLEEPING;
        case 'D': return PSUTIL_STATUS_DISK_SLEEP;
        case 'Z': return PSUTIL_STATUS_ZOMBIE;
        case 'T': return PSUTIL_STATUS_STOPPED;
        case 't': return PSUTIL_STATUS_TRACING_STOP;
        case 'X':
        case 'x': return PSUTIL_STATUS_DEAD;
        case 'I': return PSUTIL_STATUS_IDLE;
        case 'W': return PSUTIL_STATUS_WAKING;      // paging (2.6 only)
        case 'K': return PSUTIL_STATUS_WAKE_KILL;   // wakekill
        case 'P': return PSUTIL_STATUS_PARKED;      // parked
        case 'L': return PSUTIL_STATUS_LOCKED;      // has pinned pages
        default:  return PSUTIL_STATUS_RUNNING;
    }
}

const char* psutil_linux_process_get_username(Process* proc) {
    char path[64];

    if (proc == NULL) {
        return NULL;
    }
    if (proc->username != NULL) {
        return proc->username;
    }

    snprintf(path, sizeof(path), "/proc/%d/status", (int)proc->pid);
    {
        FILE* fp = fopen(path, "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    unsigned int uid = 0;
                    if (sscanf(line, "Uid: %u", &uid) == 1) {
                        struct passwd *pwd = getpwuid((uid_t)uid);
                        if (pwd != NULL) {
                            proc->username = strdup(pwd->pw_name);
                        }
                    }
                    break;
                }
            }
            fclose(fp);
        }
    }
    return proc->username;
}

double psutil_linux_process_get_create_time(Process* proc) {
    psutil_linux_stat* st;

    if (proc == NULL) {
        return 0.0;
    }
    if (proc->create_time > 0.0) {
        return proc->create_time;
    }

    st = psutil_linux_stat_get(proc, 0);
    if (st != NULL && st->valid) {
        proc->create_time = psutil_linux_boot_time_cached() +
                            (double)st->starttime / (double)psutil_linux_clk_tck();
    }
    return proc->create_time;
}

const char* psutil_linux_process_get_cwd(Process* proc) {
    char path[64];

    if (proc == NULL) {
        return NULL;
    }
    if (proc->cwd != NULL) {
        return proc->cwd;
    }

    snprintf(path, sizeof(path), "/proc/%d/cwd", (int)proc->pid);
    {
        char cwd[4096];
        ssize_t len = readlink(path, cwd, sizeof(cwd) - 1);
        if (len > 0) {
            cwd[len] = '\0';
            proc->cwd = strdup(cwd);
        } else {
            psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        }
    }
    return proc->cwd;
}

int psutil_linux_process_get_nice(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    return getpriority(PRIO_PROCESS, proc->pid);
}

int psutil_linux_process_set_nice(Process* proc, int value) {
    if (proc == NULL) {
        return -1;
    }
    return setpriority(PRIO_PROCESS, proc->pid, value);
}

psutil_uids psutil_linux_process_get_uids(Process* proc) {
    psutil_uids uids = {0};
    if (proc == NULL) {
        return uids;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return uids;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            unsigned int real_uid = 0, eff_uid = 0, saved_uid = 0, fs_uid = 0;
            if (sscanf(line, "Uid:\t%u\t%u\t%u\t%u",
                       &real_uid, &eff_uid, &saved_uid, &fs_uid) >= 1) {
                uids.real = real_uid;
                uids.effective = eff_uid;
                uids.saved = saved_uid;
                uids.setuid = fs_uid;
            }
            break;
        }
    }
    fclose(fp);
    return uids;
}

psutil_gids psutil_linux_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    if (proc == NULL) {
        return gids;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return gids;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Gid:", 4) == 0) {
            unsigned int real_gid = 0, eff_gid = 0, saved_gid = 0, fs_gid = 0;
            if (sscanf(line, "Gid:\t%u\t%u\t%u\t%u",
                       &real_gid, &eff_gid, &saved_gid, &fs_gid) >= 1) {
                gids.real = real_gid;
                gids.effective = eff_gid;
                gids.saved = saved_gid;
                gids.setgid = fs_gid;
            }
            break;
        }
    }
    fclose(fp);
    return gids;
}

const char* psutil_linux_process_get_terminal(Process* proc) {
    psutil_linux_stat* st;

    if (proc == NULL) {
        return NULL;
    }
    st = psutil_linux_stat_get(proc, 0);
    if (st == NULL) {
        return NULL;
    }

    if (!st->terminal_valid) {
        // /proc/<pid>/fd/0 is authoritative: it already resolves to
        // /dev/pts/N, /dev/ttyN or /dev/console. The old major/minor
        // guessing produced wrong names for ptys outside the 0..255 range.
        char path[64];
        char target[128];
        ssize_t len;

        st->terminal[0] = '\0';
        snprintf(path, sizeof(path), "/proc/%d/fd/0", (int)proc->pid);
        len = readlink(path, target, sizeof(target) - 1);
        if (len > 0) {
            target[len] = '\0';
            if (strncmp(target, "/dev/", 5) == 0 &&
                (strstr(target, "tty") != NULL ||
                 strstr(target, "pts") != NULL ||
                 strstr(target, "console") != NULL)) {
                memcpy(st->terminal, target, sizeof(st->terminal) - 1);
            }
        }
        st->terminal_valid = 1;
    }

    if (st->terminal[0] == '\0') {
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return NULL;
    }
    return st->terminal;
}

int psutil_linux_process_get_num_fds(Process* proc) {
    char path[64];
    DIR* dir;
    int count = 0;
    struct dirent* entry;

    if (proc == NULL) {
        return 0;
    }

    snprintf(path, sizeof(path), "/proc/%d/fd", (int)proc->pid);
    dir = opendir(path);
    if (dir == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    closedir(dir);
    return count;
}

psutil_io_counters psutil_linux_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    char path[64];
    FILE* fp;
    char line[256];

    if (proc == NULL) {
        return counters;
    }

    snprintf(path, sizeof(path), "/proc/%d/io", (int)proc->pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return counters;
    }

    // /proc/<pid>/io fields, mapped the same way psutil maps them:
    //   syscr -> read_count        syscw    -> write_count
    //   read_bytes -> read_bytes   write_bytes -> write_bytes
    // rchar/wchar (chars incl. page cache hits) are intentionally not
    // reported as *bytes*; that mapping was wrong before.
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long value = 0;
        if (sscanf(line, "read_bytes: %llu", &value) == 1 ||
            sscanf(line, "read_bytes:  %llu", &value) == 1) {
            counters.read_bytes = value;
        } else if (sscanf(line, "write_bytes: %llu", &value) == 1) {
            counters.write_bytes = value;
        } else if (sscanf(line, "syscr: %llu", &value) == 1) {
            counters.read_count = value;
        } else if (sscanf(line, "syscw: %llu", &value) == 1) {
            counters.write_count = value;
        }
    }
    fclose(fp);
    return counters;
}

int psutil_linux_process_get_ionice(Process* proc) {
    long ret;

    if (proc == NULL) {
        return -1;
    }
    errno = 0;
    ret = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, (long)proc->pid);
    if (ret < 0) {
        psutil_set_last_error(errno == EPERM ? PSUTIL_ERROR_PERMISSION
                                             : PSUTIL_ERROR_NOT_FOUND);
        return -1;
    }
    return (int)(ret >> IOPRIO_CLASS_SHIFT);
}

int psutil_linux_process_set_ionice(Process* proc, int ioclass, int value) {
    long ioprio;

    // IOPRIO_CLASS_NONE(0) .. IOPRIO_CLASS_IDLE(3), data 0..7
    if (proc == NULL || ioclass < 0 || ioclass > 3 || value < 0 || value > 7) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return -1;
    }
    ioprio = IOPRIO_PRIO_VALUE(ioclass, value);
    return (int)syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, (long)proc->pid, ioprio);
}

// Returns the number of CPUs the kernel knows about (possible), which is
// what sched_getaffinity() uses to size the cpumask. Using the *online*
// count makes the kernel reject the mask with EINVAL when some CPUs are
// offline.
static int psutil_linux_possible_cpus(void) {
    int n = (int)sysconf(_SC_NPROCESSORS_CONF);
    return n > 0 ? n : 1;
}

static size_t psutil_linux_cpumask_size(void) {
    size_t bits = (size_t)psutil_linux_possible_cpus();
    size_t bytes = (bits + 7) / 8;
    size_t align = sizeof(unsigned long);
    return (bytes + align - 1) / align * align;
}

int* psutil_linux_process_get_cpu_affinity(Process* proc, int* count) {
    size_t mask_size = psutil_linux_cpumask_size();
    int possible = psutil_linux_possible_cpus();
    unsigned char* mask;
    int* cpus;
    int index = 0;
    int i;

    *count = 0;
    if (proc == NULL) {
        return NULL;
    }

    // The kernel validates the mask length against nr_cpu_ids (possible
    // CPUs), not against the number of online ones.
    mask = (unsigned char*)calloc(mask_size, 1);
    if (mask == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }
    if (sched_getaffinity(proc->pid, mask_size, (cpu_set_t*)mask) != 0) {
        free(mask);
        psutil_set_last_error(errno == EPERM ? PSUTIL_ERROR_PERMISSION
                                             : PSUTIL_ERROR_NOT_FOUND);
        return NULL;
    }

    cpus = (int*)malloc((size_t)possible * sizeof(int));
    if (cpus == NULL) {
        free(mask);
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }
    for (i = 0; i < possible; i++) {
        if (mask[i / 8] & (unsigned char)(1u << (i % 8))) {
            cpus[index++] = i;
        }
    }

    free(mask);
    *count = index;
    return cpus;
}

int psutil_linux_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    size_t mask_size;
    int possible;
    unsigned char* mask;
    int i;
    int result;

    if (proc == NULL || cpus == NULL || count <= 0) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return -1;
    }

    possible = psutil_linux_possible_cpus();
    mask_size = psutil_linux_cpumask_size();
    mask = (unsigned char*)calloc(mask_size, 1);
    if (mask == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return -1;
    }

    for (i = 0; i < count; i++) {
        int cpu = cpus[i];
        if (cpu < 0 || cpu >= possible) {
            free(mask);
            psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
            return -1;
        }
        mask[cpu / 8] |= (unsigned char)(1u << (cpu % 8));
    }

    result = sched_setaffinity(proc->pid, mask_size, (cpu_set_t*)mask) == 0 ? 0 : -1;
    if (result != 0) {
        psutil_set_last_error(errno == EPERM ? PSUTIL_ERROR_PERMISSION
                                             : PSUTIL_ERROR_NOT_FOUND);
    }
    free(mask);
    return result;
}

int psutil_linux_process_get_cpu_num(Process* proc) {
    psutil_linux_stat* st = psutil_linux_stat_get(proc, 0);
    if (st == NULL || !st->valid) {
        return -1;
    }
    return st->cpu_num;
}
char** psutil_linux_process_get_environ(Process* proc, int* count) {
    char path[64];
    char* buf;
    size_t len = 0;
    size_t pos;
    int nvars = 0;
    int in_var = 0;
    int index = 0;
    char** out;

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    snprintf(path, sizeof(path), "/proc/%d/environ", (int)proc->pid);
    // Read the whole block: a fixed 4096 buffer truncated long environments,
    // and when the block filled the buffer the terminator scan walked off
    // the end (stack-buffer-overflow reported by ASan).
    buf = read_file_all(path, &len);
    if (buf == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }
    if (len == 0) {
        free(buf);
        return NULL;
    }

    for (pos = 0; pos < len; pos++) {
        if (buf[pos] == '\0') {
            in_var = 0;
        } else if (!in_var) {
            in_var = 1;
            nvars++;
        }
    }

    out = (char**)calloc((size_t)nvars + 1, sizeof(char*));
    if (out == NULL) {
        free(buf);
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }

    pos = 0;
    while (pos < len && index < nvars) {
        size_t seg;
        if (buf[pos] == '\0') {
            pos++;
            continue;
        }
        seg = strnlen(buf + pos, len - pos);
        out[index] = (char*)malloc(seg + 1);
        if (out[index] == NULL) {
            psutil_free_string_array(out, index);
            free(buf);
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
        memcpy(out[index], buf + pos, seg);
        out[index][seg] = '\0';
        index++;
        pos += seg;
    }
    out[index] = NULL;

    free(buf);
    *count = index;
    return out;
}

int psutil_linux_process_get_num_handles(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    // On Linux, handles are file descriptors
    return psutil_linux_process_get_num_fds(proc);
}

psutil_ctx_switches psutil_linux_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    if (proc == NULL) {
        return switches;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return switches;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "voluntary_ctxt_switches:", 23) == 0) {
            sscanf(line, "voluntary_ctxt_switches:	%" SCNu64, &switches.voluntary);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 25) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches:	%" SCNu64, &switches.involuntary);
        }
    }
    
    fclose(fp);
    return switches;
}

int psutil_linux_process_get_num_threads(Process* proc) {
    psutil_linux_stat* st = psutil_linux_stat_get(proc, 0);
    if (st == NULL || !st->valid) {
        return 0;
    }
    return (int)st->num_threads;
}

psutil_thread* psutil_linux_process_get_threads(Process* proc, int* count) {
    char dirpath[64];
    DIR* dir;
    struct dirent* entry;
    psutil_thread* threads = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;
    if (proc == NULL) {
        return NULL;
    }

    snprintf(dirpath, sizeof(dirpath), "/proc/%d/task", (int)proc->pid);
    dir = opendir(dirpath);
    if (dir == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        char stat_path[96];
        char* endp = NULL;
        long tid = strtol(entry->d_name, &endp, 10);
        char* data;
        psutil_linux_stat tmp;

        if (tid <= 0 || endp == NULL || *endp != '\0') {
            continue;
        }
        if (index >= capacity) {
            int ncap = capacity == 0 ? 32 : capacity * 2;
            psutil_thread* nt = (psutil_thread*)realloc(
                threads, (size_t)ncap * sizeof(psutil_thread));
            if (nt == NULL) {
                free(threads);
                closedir(dir);
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            threads = nt;
            capacity = ncap;
        }

        threads[index].id = (uint32_t)tid;
        threads[index].user_time = 0.0;
        threads[index].system_time = 0.0;

        snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%ld/stat",
                 (int)proc->pid, tid);
        data = read_file_all(stat_path, NULL);
        if (data != NULL) {
            char* close_paren = strrchr(data, ')');
            memset(&tmp, 0, sizeof(tmp));
            if (close_paren != NULL && psutil_linux_parse_stat(close_paren + 1, &tmp)) {
                threads[index].user_time =
                    (double)tmp.utime / (double)psutil_linux_clk_tck();
                threads[index].system_time =
                    (double)tmp.stime / (double)psutil_linux_clk_tck();
            }
            free(data);
        }
        index++;
    }
    closedir(dir);

    *count = index;
    return threads;
}

psutil_cpu_times psutil_linux_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    psutil_linux_stat* st = psutil_linux_stat_get(proc, 0);

    if (st == NULL || !st->valid) {
        return times;
    }

    times.user = (double)st->utime / (double)psutil_linux_clk_tck();
    times.system = (double)st->stime / (double)psutil_linux_clk_tck();
    times.children_user = (double)st->cutime / (double)psutil_linux_clk_tck();
    times.children_system = (double)st->cstime / (double)psutil_linux_clk_tck();
    times.idle = 0.0;
    return times;
}

psutil_memory_info psutil_linux_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    char path[64];
    FILE* fp;

    if (proc == NULL) {
        return info;
    }

    snprintf(path, sizeof(path), "/proc/%d/statm", (int)proc->pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_NOT_FOUND);
        return info;
    }

    {
        unsigned long size = 0, resident = 0, share = 0;
        unsigned long text = 0, lib = 0, data = 0, dt = 0;
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            page_size = 4096;
        }
        // statm: size resident shared text lib data dt (all in pages).
        // These also feed memory_percent("text"/"lib"/"data"), which
        // always returned 0 before because they were never stored.
        if (fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu",
                   &size, &resident, &share, &text, &lib, &data, &dt) == 7) {
            info.vms = (uint64_t)size * (uint64_t)page_size;
            info.rss = (uint64_t)resident * (uint64_t)page_size;
            info.shared = (uint64_t)share * (uint64_t)page_size;
            info.text = (uint64_t)text * (uint64_t)page_size;
            info.lib = (uint64_t)lib * (uint64_t)page_size;
            info.data = (uint64_t)data * (uint64_t)page_size;
            info.dirty = (uint64_t)dt * (uint64_t)page_size;
        } else {
            psutil_set_last_error(PSUTIL_ERROR_IO);
        }
    }
    fclose(fp);
    return info;
}

psutil_memory_info psutil_linux_process_get_memory_full_info(Process* proc) {
    psutil_memory_info info = psutil_linux_process_get_memory_info(proc);
    char path[64];
    FILE* fp;

    if (proc == NULL) {
        return info;
    }

    // smaps_rollup is pre-aggregated by the kernel (4.14+), so this stays
    // cheap compared to walking every VMA in /proc/<pid>/smaps.
    snprintf(path, sizeof(path), "/proc/%d/smaps_rollup", (int)proc->pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return info;
    }

    {
        char line[256];
        uint64_t private_clean = 0;
        uint64_t private_dirty = 0;
        uint64_t pss = 0;
        while (fgets(line, sizeof(line), fp) != NULL) {
            unsigned long value = 0;
            if (sscanf(line, "Pss: %lu kB", &value) == 1) {
                pss = (uint64_t)value * 1024;
            } else if (sscanf(line, "Private_Clean: %lu kB", &value) == 1) {
                private_clean = (uint64_t)value * 1024;
            } else if (sscanf(line, "Private_Dirty: %lu kB", &value) == 1) {
                private_dirty = (uint64_t)value * 1024;
            }
        }
        info.pss = pss;
        // uss = unshared memory = private clean + private dirty
        info.uss = private_clean + private_dirty;
    }
    fclose(fp);
    return info;
}

double psutil_linux_process_get_memory_percent(Process* proc, const char* memtype) {
    psutil_memory_info proc_mem;
    psutil_memory_info system_mem;
    uint64_t proc_value;
    const char* type = (memtype != NULL) ? memtype : "rss";

    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return 0.0;
    }

    if (strcmp(type, "uss") == 0 || strcmp(type, "pss") == 0) {
        proc_mem = psutil_linux_process_get_memory_full_info(proc);
    } else {
        proc_mem = psutil_linux_process_get_memory_info(proc);
    }
    system_mem = psutil_linux_virtual_memory();
    if (system_mem.vms == 0) {
        return 0.0;
    }

    if (strcmp(type, "vms") == 0) {
        proc_value = proc_mem.vms;
    } else if (strcmp(type, "shared") == 0) {
        proc_value = proc_mem.shared;
    } else if (strcmp(type, "text") == 0) {
        proc_value = proc_mem.text;
    } else if (strcmp(type, "lib") == 0) {
        proc_value = proc_mem.lib;
    } else if (strcmp(type, "data") == 0) {
        proc_value = proc_mem.data;
    } else if (strcmp(type, "uss") == 0) {
        proc_value = proc_mem.uss;
    } else if (strcmp(type, "pss") == 0) {
        proc_value = proc_mem.pss;
    } else if (strcmp(type, "dirty") == 0) {
        proc_value = proc_mem.dirty;
    } else {
        proc_value = proc_mem.rss;
    }

    return (double)proc_value / (double)system_mem.vms * 100.0;
}
static void psutil_linux_smaps_header_path(const char* line, char* out, size_t outsz) {
    int field = 0;
    const char* p = line;

    out[0] = '\0';
    while (*p != '\0') {
        const char* e;
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0' || *p == '\n') {
            break;
        }
        e = p;
        while (*e != '\0' && *e != ' ' && *e != '\n') {
            e++;
        }
        field++;
        if (field == 6) {  // addr perms offset dev inode pathname
            size_t n = (size_t)(e - p);
            if (n >= outsz) {
                n = outsz - 1;
            }
            memcpy(out, p, n);
            out[n] = '\0';
            return;
        }
        p = e;
    }
}

static int psutil_linux_map_append(psutil_memory_map** maps, int* capacity,
                                   int* index, const psutil_memory_map* m,
                                   int grouped) {
    // grouped=True merges consecutive regions backed by the same path,
    // which is what psutil's grouped parameter does.
    if (grouped && *index > 0 &&
        strcmp((*maps)[*index - 1].path, m->path) == 0) {
        psutil_memory_map* last = &(*maps)[*index - 1];
        last->rss += m->rss;
        last->vms += m->vms;
        last->shared += m->shared;
        last->text += m->text;
        last->lib += m->lib;
        last->data += m->data;
        last->dirty += m->dirty;
        return 0;
    }
    if (*index >= *capacity) {
        int ncap = (*capacity == 0) ? 64 : (*capacity * 2);
        psutil_memory_map* nm = (psutil_memory_map*)realloc(
            *maps, (size_t)ncap * sizeof(psutil_memory_map));
        if (nm == NULL) {
            return -1;
        }
        *maps = nm;
        *capacity = ncap;
    }
    (*maps)[(*index)++] = *m;
    return 0;
}

psutil_memory_map* psutil_linux_process_get_memory_maps(Process* proc, int* count, int grouped) {
    char path[64];
    FILE* fp;
    psutil_memory_map* maps = NULL;
    psutil_memory_map cur;
    int capacity = 0;
    int index = 0;
    int have_cur = 0;
    char line[1024];

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    snprintf(path, sizeof(path), "/proc/%d/smaps", (int)proc->pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }

    memset(&cur, 0, sizeof(cur));

    while (fgets(line, sizeof(line), fp) != NULL) {
        int is_header = ((line[0] >= '0' && line[0] <= '9') ||
                         (line[0] >= 'a' && line[0] <= 'f')) &&
                        strchr(line, '-') != NULL;

        if (is_header) {
            // Flush the previous region *before* starting a new one. The old
            // code required index > 0, which silently dropped the first
            // mapping of every process.
            if (have_cur) {
                if (psutil_linux_map_append(&maps, &capacity, &index, &cur,
                                            grouped) != 0) {
                    free(maps);
                    fclose(fp);
                    psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                    return NULL;
                }
            }
            memset(&cur, 0, sizeof(cur));
            psutil_linux_smaps_header_path(line, cur.path, sizeof(cur.path));
            if (cur.path[0] == '\0') {
                snprintf(cur.path, sizeof(cur.path), "[anon]");
            }
            have_cur = 1;
        } else if (have_cur) {
            unsigned long value = 0;
            if (sscanf(line, "Size: %lu kB", &value) == 1) {
                cur.vms = (uint64_t)value * 1024;
            } else if (sscanf(line, "Rss: %lu kB", &value) == 1) {
                cur.rss = (uint64_t)value * 1024;
            } else if (sscanf(line, "Shared_Clean: %lu kB", &value) == 1 ||
                       sscanf(line, "Shared_Dirty: %lu kB", &value) == 1) {
                cur.shared += (uint64_t)value * 1024;
            } else if (sscanf(line, "Private_Dirty: %lu kB", &value) == 1) {
                cur.dirty += (uint64_t)value * 1024;
            }
        }
    }

    if (have_cur) {
        if (psutil_linux_map_append(&maps, &capacity, &index, &cur, grouped) != 0) {
            free(maps);
            fclose(fp);
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
    }

    fclose(fp);
    *count = index;
    return maps;
}
psutil_open_file* psutil_linux_process_get_open_files(Process* proc, int* count) {
    char fd_dir[64];
    DIR* dir;
    struct dirent* entry;
    psutil_open_file* files = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", (int)proc->pid);
    dir = opendir(fd_dir);
    if (dir == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        char link_path[96];
        char target[PATH_MAX];
        char* endp = NULL;
        long fd;
        ssize_t len;
        struct stat st;

        if (entry->d_name[0] == '.') {
            continue;
        }
        fd = strtol(entry->d_name, &endp, 10);
        if (fd < 0 || endp == NULL || *endp != '\0') {
            continue;
        }

        snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%ld", (int)proc->pid, fd);
        len = readlink(link_path, target, sizeof(target) - 1);
        if (len <= 0) {
            continue;
        }
        target[len] = '\0';

        // Only regular files reachable through an absolute path count;
        // sockets, pipes, anon inodes and deleted files are skipped.
        if (target[0] != '/') {
            continue;
        }
        if (stat(target, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        if (index >= capacity) {
            int ncap = (capacity == 0) ? 64 : (capacity * 2);
            psutil_open_file* nf = (psutil_open_file*)realloc(
                files, (size_t)ncap * sizeof(psutil_open_file));
            if (nf == NULL) {
                free(files);
                closedir(dir);
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            files = nf;
            capacity = ncap;
        }

        // snprintf always terminates; the old strncpy(path, n-1) could leave
        // the last byte unterminated for long paths.
        strncpy(files[index].path, target, sizeof(files[index].path) - 1);
        files[index].path[sizeof(files[index].path) - 1] = '\0';
        files[index].fd = (int)fd;
        index++;
    }

    closedir(dir);
    *count = index;
    return files;
}
psutil_net_connection* psutil_linux_process_get_net_connections(Process* proc, const char* kind, int* count) {
    unsigned long* inodes = NULL;
    int* fds = NULL;
    int inode_count;
    psutil_net_connection* conns;

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    inode_count = collect_socket_inodes((pid_t)proc->pid, &inodes, &fds);
    if (inode_count < 0) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }
    if (inode_count == 0) {
        free(inodes);
        free(fds);
        return NULL;
    }

    conns = psutil_linux_collect_net(kind, inodes, fds, inode_count, count);
    free(inodes);
    free(fds);
    return conns;
}
int psutil_linux_process_send_signal(Process* proc, int sig) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, sig);
}

int psutil_linux_process_suspend(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGSTOP);
}

int psutil_linux_process_resume(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGCONT);
}

int psutil_linux_process_terminate(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGTERM);
}

int psutil_linux_process_kill(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGKILL);
}

int psutil_linux_process_wait(Process* proc, double timeout) {
    pid_t pid;
    pid_t r = -1;
    int status = 0;

    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return -1;
    }
    pid = (pid_t)proc->pid;

    if (timeout < 0) {
        r = waitpid(pid, &status, 0);
    } else {
        double deadline = psutil_monotonic_now() + timeout;
        for (;;) {
            r = waitpid(pid, &status, WNOHANG);
            if (r == pid || r < 0) {
                break;
            }
            if (psutil_monotonic_now() >= deadline) {
                return 1;  // still running when the timeout expired
            }
            usleep(20000);
        }
    }

    if (r == pid) {
        return 0;
    }
    if (r < 0 && errno == ECHILD) {
        // The target is not our child (very common: an unrelated PID).
        // Fall back to polling its liveness instead of reporting a failure.
        if (timeout < 0) {
            while (psutil_linux_process_is_running(proc)) {
                usleep(20000);
            }
            return 0;
        } else {
            double deadline = psutil_monotonic_now() + timeout;
            while (psutil_linux_process_is_running(proc)) {
                if (psutil_monotonic_now() >= deadline) {
                    return 1;
                }
                usleep(20000);
            }
            return 0;
        }
    }

    psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
    return -1;
}

int psutil_linux_process_is_running(Process* proc) {
    psutil_linux_stat* st;

    if (proc == NULL) {
        return 0;
    }
    st = psutil_linux_stat_get(proc, 1);
    if (st == NULL || !st->valid) {
        return 0;
    }
    // Zombie / dead processes are not "running" from psutil's point of view.
    return st->state != 'Z' && st->state != 'X';
}

// ---------------------------------------------------------------- system

// Reads the aggregate "cpu" line of /proc/stat. Fields, in order:
// user nice system idle iowait irq softirq steal.
#define PSUTIL_LINUX_CPU_FIELDS 8

static int psutil_linux_cpu_raw(unsigned long long f[PSUTIL_LINUX_CPU_FIELDS]) {
    FILE* fp;
    char line[256];
    int ok = -1;

    memset(f, 0, PSUTIL_LINUX_CPU_FIELDS * sizeof(unsigned long long));
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return -1;
    }
    if (fgets(line, sizeof(line), fp) != NULL &&
        strncmp(line, "cpu ", 4) == 0) {
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6],
                   &f[7]) >= 4) {
            ok = 0;
        }
    }
    fclose(fp);
    return ok;
}

psutil_memory_info psutil_linux_virtual_memory(void) {
    psutil_memory_info info = {0};
    FILE* fp;
    char line[256];
    uint64_t mem_total = 0, mem_free = 0, mem_available = 0;
    uint64_t buffers = 0, cached = 0, sreclaim = 0, shmem = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return info;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long kb = 0;
        if (sscanf(line, "MemTotal: %llu kB", &kb) == 1) {
            mem_total = (uint64_t)kb * 1024;
        } else if (sscanf(line, "MemFree: %llu kB", &kb) == 1) {
            mem_free = (uint64_t)kb * 1024;
        } else if (sscanf(line, "MemAvailable: %llu kB", &kb) == 1) {
            mem_available = (uint64_t)kb * 1024;
        } else if (sscanf(line, "Buffers: %llu kB", &kb) == 1) {
            buffers = (uint64_t)kb * 1024;
        } else if (sscanf(line, "Cached: %llu kB", &kb) == 1) {
            cached = (uint64_t)kb * 1024;
        } else if (sscanf(line, "SReclaimable: %llu kB", &kb) == 1) {
            sreclaim = (uint64_t)kb * 1024;
        } else if (sscanf(line, "Shmem: %llu kB", &kb) == 1) {
            shmem = (uint64_t)kb * 1024;
        }
    }
    fclose(fp);

    if (mem_total == 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return info;
    }

    // MemAvailable (fallback: free + buffers + cached) is what psutil uses;
    // sysinfo()'s totalram - freeram badly overestimates "used" because it
    // ignores page cache.
    if (mem_available == 0) {
        mem_available = mem_free + buffers + cached;
    }
    info.vms = mem_total;
    info.available = mem_available;
    info.rss = (mem_total > mem_available) ? (mem_total - mem_available) : 0;
    info.data = buffers + cached + sreclaim;
    info.shared = shmem;
    return info;
}

psutil_memory_info psutil_linux_swap_memory(void) {
    psutil_memory_info info = {0};
    FILE* fp;
    char line[256];
    uint64_t total = 0, free_mem = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return info;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long kb = 0;
        if (sscanf(line, "SwapTotal: %llu kB", &kb) == 1) {
            total = (uint64_t)kb * 1024;
        } else if (sscanf(line, "SwapFree: %llu kB", &kb) == 1) {
            free_mem = (uint64_t)kb * 1024;
        }
    }
    fclose(fp);

    info.vms = total;
    info.rss = (total > free_mem) ? (total - free_mem) : 0;
    return info;
}

psutil_cpu_times psutil_linux_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    unsigned long long f[PSUTIL_LINUX_CPU_FIELDS];
    long tck = psutil_linux_clk_tck();

    (void)percpu;  // the API returns a single aggregate sample
    if (psutil_linux_cpu_raw(f) != 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return times;
    }

    // The struct has no separate "nice"/"iowait" members, so nice is folded
    // into user and iowait into idle; the totals still add up.
    times.user = (double)(f[0] + f[1]) / (double)tck;
    times.system = (double)(f[2] + f[5] + f[6]) / (double)tck;
    times.idle = (double)(f[3] + f[4]) / (double)tck;
    return times;
}

static double psutil_linux_cpu_busy_delta(const unsigned long long a[PSUTIL_LINUX_CPU_FIELDS],
                                          const unsigned long long b[PSUTIL_LINUX_CPU_FIELDS],
                                          double* total_delta) {
    unsigned long long ta = 0, tb = 0;
    unsigned long long ia = 0, ib = 0;
    int i;

    for (i = 0; i < PSUTIL_LINUX_CPU_FIELDS; i++) {
        ta += a[i];
        tb += b[i];
    }
    ia = a[3] + a[4];  // idle + iowait
    ib = b[3] + b[4];

    *total_delta = (double)(tb - ta);
    if (tb <= ta) {
        *total_delta = 0.0;
        return 0.0;
    }
    return (double)((tb - ta) - (ib - ia));
}

double psutil_linux_cpu_percent(double interval, int percpu) {
    static unsigned long long prev[PSUTIL_LINUX_CPU_FIELDS];
    static int prev_valid = 0;
    unsigned long long a[PSUTIL_LINUX_CPU_FIELDS];
    unsigned long long b[PSUTIL_LINUX_CPU_FIELDS];
    double busy_delta = 0.0;
    double total_delta = 0.0;

    (void)percpu;
    if (psutil_linux_cpu_raw(a) != 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return 0.0;
    }

    if (interval > 0) {
        usleep((useconds_t)(interval * 1000000.0));
        if (psutil_linux_cpu_raw(b) != 0) {
            return 0.0;
        }
    } else {
        // Non-blocking: compare against the previous call of this function.
        if (!prev_valid) {
            memcpy(prev, a, sizeof(prev));
            prev_valid = 1;
            return 0.0;
        }
        memcpy(b, a, sizeof(b));
        memcpy(a, prev, sizeof(a));
    }
    memcpy(prev, b, sizeof(prev));
    prev_valid = 1;

    busy_delta = psutil_linux_cpu_busy_delta(a, b, &total_delta);
    if (total_delta <= 0.0) {
        return 0.0;
    }
    // Busy / all-CPU total, so the result is 0..100 regardless of the number
    // of cores. The old code divided busy seconds by the interval, which
    // returned 800% on an 8-core box.
    return (busy_delta / total_delta) * 100.0;
}

psutil_cpu_times psutil_linux_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    static unsigned long long prev[PSUTIL_LINUX_CPU_FIELDS];
    static int prev_valid = 0;
    unsigned long long a[PSUTIL_LINUX_CPU_FIELDS];
    unsigned long long b[PSUTIL_LINUX_CPU_FIELDS];
    double total_delta = 0.0;
    long tck = psutil_linux_clk_tck();

    (void)percpu;
    (void)tck;
    if (psutil_linux_cpu_raw(a) != 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return times;
    }

    if (interval > 0) {
        usleep((useconds_t)(interval * 1000000.0));
        if (psutil_linux_cpu_raw(b) != 0) {
            return times;
        }
    } else {
        if (!prev_valid) {
            memcpy(prev, a, sizeof(prev));
            prev_valid = 1;
            return times;
        }
        memcpy(b, a, sizeof(b));
        memcpy(a, prev, sizeof(a));
    }
    memcpy(prev, b, sizeof(prev));
    prev_valid = 1;

    (void)psutil_linux_cpu_busy_delta(a, b, &total_delta);
    if (total_delta <= 0.0) {
        return times;
    }

    // Every bucket is a share of the *whole* delta, not of busy time only,
    // so user + system + idle now add up to 100% (it used to be 100%
    // between user and system alone).
    times.user = (double)(b[0] - a[0] + b[1] - a[1]) / total_delta * 100.0;
    times.system = (double)(b[2] - a[2] + b[5] - a[5] + b[6] - a[6]) / total_delta * 100.0;
    times.idle = (double)(b[3] - a[3] + b[4] - a[4]) / total_delta * 100.0;
    return times;
}

int psutil_linux_cpu_count(int logical) {
    FILE* fp;
    char line[256];
    int cores = 0;
    int cur_core = -1;
    int cur_socket = -1;
    int seen_core = -1;
    int seen_socket = -1;

    if (logical) {
        int n = sysconf(_SC_NPROCESSORS_ONLN);
        return n > 0 ? n : 0;
    }

    // Physical cores = unique (socket, core id) pairs. Counting "max core id
    // + 1" was wrong on any box where core ids are not 0..N-1.
    fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        int value = 0;
        if (sscanf(line, "physical id : %d", &value) == 1) {
            cur_socket = value;
            continue;
        }
        if (sscanf(line, "core id : %d", &value) == 1) {
            cur_core = value;
            continue;
        }
        if (line[0] == '\n' || strncmp(line, "processor", 9) == 0) {
            if (cur_core >= 0) {
                if (cur_socket != seen_socket || cur_core != seen_core) {
                    cores++;
                    seen_socket = cur_socket;
                    seen_core = cur_core;
                }
            }
            if (strncmp(line, "processor", 9) == 0) {
                cur_core = -1;
                cur_socket = -1;
            }
        }
    }
    fclose(fp);

    return cores;
}

psutil_cpu_stats psutil_linux_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    FILE* fp;
    char line[256];

    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return stats;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "ctxt", 4) == 0) {
            sscanf(line, "ctxt %" SCNu64, &stats.ctx_switches);
        } else if (strncmp(line, "intr", 4) == 0) {
            sscanf(line, "intr %" SCNu64, &stats.interrupts);
        } else if (strncmp(line, "softirq", 7) == 0) {
            sscanf(line, "softirq %" SCNu64, &stats.soft_interrupts);
        }
    }
    fclose(fp);
    // /proc/stat has no total syscall counter on Linux (only per-process
    // ones), so syscalls stays 0, which is also what psutil reports.
    return stats;
}

psutil_io_counters psutil_linux_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};
    FILE* fp;
    char line[512];

    (void)pernic;  // psutil_io_counters carries no per-nic breakdown
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return counters;
    }

    fgets(line, sizeof(line), fp);  // two header lines
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) != NULL) {
        char iface[64];
        unsigned long long rx_bytes = 0, rx_packets = 0, rx_errs = 0, rx_drop = 0;
        unsigned long long rx_fifo = 0, rx_frame = 0, rx_compressed = 0, rx_multicast = 0;
        unsigned long long tx_bytes = 0, tx_packets = 0, tx_errs = 0, tx_drop = 0;
        unsigned long long tx_fifo = 0, tx_colls = 0, tx_carrier = 0, tx_compressed = 0;
        char* colon = strchr(line, ':');

        if (colon == NULL) {
            continue;
        }
        *colon = '\0';
        strncpy(iface, line, sizeof(iface) - 1);
        iface[sizeof(iface) - 1] = '\0';
        {
            char* p = iface;
            while (*p == ' ') {
                p++;
            }
            memmove(iface, p, strlen(p) + 1);
        }

        // Loopback is included: psutil counts it too.
        if (sscanf(colon + 1,
                   "%llu %llu %llu %llu %llu %llu %llu %llu "
                   "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo,
                   &rx_frame, &rx_compressed, &rx_multicast, &tx_bytes,
                   &tx_packets, &tx_errs, &tx_drop, &tx_fifo, &tx_colls,
                   &tx_carrier, &tx_compressed) != 16) {
            continue;
        }

        counters.read_bytes += rx_bytes;
        counters.write_bytes += tx_bytes;
        counters.read_count += rx_packets;
        counters.write_count += tx_packets;
    }

    fclose(fp);
    return counters;
}
// Helper function to parse TCP state from /proc/net/tcp
static int parse_tcp_state(int state) {
    switch (state) {
        case 0x1: return PSUTIL_CONN_ESTABLISHED;
        case 0x2: return PSUTIL_CONN_SYN_SENT;
        case 0x3: return PSUTIL_CONN_SYN_RECV;
        case 0x4: return PSUTIL_CONN_FIN_WAIT1;
        case 0x5: return PSUTIL_CONN_FIN_WAIT2;
        case 0x6: return PSUTIL_CONN_TIME_WAIT;
        case 0x7: return PSUTIL_CONN_CLOSE;
        case 0x8: return PSUTIL_CONN_CLOSE_WAIT;
        case 0x9: return PSUTIL_CONN_LAST_ACK;
        case 0xA: return PSUTIL_CONN_LISTEN;
        case 0xB: return PSUTIL_CONN_CLOSING;
        default: return PSUTIL_CONN_NONE;
    }
}

// /proc/net/tcp{,6} store addresses as hex, one 32-bit word at a time, each
// word in *host* byte order (little endian on x86/ARM), e.g. "0100007F" is
// 127.0.0.1. Splitting the hex string into 8-digit words and reading each
// word's bytes from low to high reproduces the address for both families.
static void psutil_linux_format_addr(char* out, size_t outsz, const char* hex,
                                     int family, unsigned int port) {
    unsigned char bytes[16];
    char text[INET6_ADDRSTRLEN];
    int words = (family == AF_INET6) ? 4 : 1;
    int i;

    memset(bytes, 0, sizeof(bytes));
    for (i = 0; i < words; i++) {
        unsigned long word = strtoul(hex + i * 8, NULL, 16);
        int b;
        for (b = 0; b < 4; b++) {
            bytes[i * 4 + b] = (unsigned char)((word >> (8 * b)) & 0xFFu);
        }
    }

    if (inet_ntop(family == AF_INET6 ? AF_INET6 : AF_INET, bytes, text,
                  sizeof(text)) == NULL) {
        snprintf(out, outsz, "?:%u", port);
        return;
    }
    snprintf(out, outsz, "%s:%u", text, port);
}

// Splits "0100007F:1F90" into the hex address part and the (hex) port and
// formats it. Returns -1 when the field is not in the expected shape.
static int psutil_linux_split_and_format(char* out, size_t outsz,
                                         const char* field, int family) {
    const char* colon = strchr(field, ':');
    char hex[40];
    size_t n;

    if (colon == NULL) {
        return -1;
    }
    n = (size_t)(colon - field);
    if (n == 0 || n >= sizeof(hex)) {
        return -1;
    }
    memcpy(hex, field, n);
    hex[n] = '\0';
    psutil_linux_format_addr(out, outsz, hex, family,
                             (unsigned int)strtoul(colon + 1, NULL, 16));
    return 0;
}

// Collects the socket inodes opened by a process. *out is allocated by this
// function (NULL when nothing was found) and released by psutil_free.
// Returns the number of inodes, or -1 when /proc/<pid>/fd is unreadable.
static int collect_socket_inodes(pid_t pid, unsigned long** out, int** out_fds) {
    char fd_dir[64];
    DIR* dir;
    struct dirent* entry;
    unsigned long* inodes = NULL;
    int* fds = NULL;
    int capacity = 0;
    int count = 0;

    *out = NULL;
    *out_fds = NULL;
    snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", (int)pid);
    dir = opendir(fd_dir);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char link_path[96];
        char target[128];
        char* endp = NULL;
        long fd;
        ssize_t len;

        if (entry->d_name[0] == '.') {
            continue;
        }
        fd = strtol(entry->d_name, &endp, 10);
        if (fd < 0 || endp == NULL || *endp != '\0') {
            continue;
        }

        snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%ld", (int)pid, fd);
        len = readlink(link_path, target, sizeof(target) - 1);
        if (len <= 0) {
            continue;
        }
        target[len] = '\0';
        // Socket symlinks look like "socket:[123456]"
        if (strncmp(target, "socket:[", 8) == 0) {
            char* close_bracket = strchr(target + 8, ']');
            unsigned long inode;
            if (close_bracket == NULL) {
                continue;
            }
            *close_bracket = '\0';
            inode = strtoul(target + 8, NULL, 10);
            if (inode == 0) {
                continue;
            }
            if (count >= capacity) {
                int ncap = (capacity == 0) ? 32 : capacity * 2;
                unsigned long* ni = (unsigned long*)realloc(
                    inodes, (size_t)ncap * sizeof(unsigned long));
                int* nf;
                if (ni == NULL) {
                    free(inodes);
                    closedir(dir);
                    return -1;
                }
                nf = (int*)realloc(fds, (size_t)ncap * sizeof(int));
                if (nf == NULL) {
                    free(ni);
                    free(fds);
                    closedir(dir);
                    return -1;
                }
                inodes = ni;
                fds = nf;
                capacity = ncap;
            }
            inodes[count] = inode;
            fds[count] = (int)fd;
            count++;
        }
    }
    closedir(dir);

    *out = inodes;
    *out_fds = fds;
    return count;
}

// Reads one of /proc/net/tcp, tcp6, udp, udp6 and appends the matching
// sockets to a dynamically grown array.
//
// The column layout is:
//   sl  local_address rem_address st tx_queue:rx_queue tr:tm->when retrnsmt
//   uid  timeout  inode
// The old code bound field 7 to `inode` and field 8 to `uid`, which are
// tx_queue and rx_queue; the real uid was dropped and the inode filter never
// matched, so per-process connection lists were always empty.
static int psutil_linux_read_net_file(const char* path, int family, int type,
                                      psutil_net_connection** conns,
                                      int* capacity, int* index,
                                      const unsigned long* filter,
                                      const int* filter_fds,
                                      int nfilter) {
    FILE* fp;
    char line[512];

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;  // e.g. IPv6 disabled: not an error
    }

    if (fgets(line, sizeof(line), fp) == NULL) {  // skip header
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long sl = 0;
        unsigned int state = 0;
        unsigned int uid = 0;
        unsigned long inode = 0;
        char lfield[48];
        char rfield[48];
        int fields;

        fields = sscanf(line,
                        " %lu: %47[0-9A-Fa-f:] %47[0-9A-Fa-f:] %x "
                        "%*s %*s %*s %u %*u %lu",
                        &sl, lfield, rfield, &state, &uid, &inode);
        if (fields < 6) {
            continue;
        }
        (void)sl;
        (void)uid;

        {
            int matched_fd = -1;
            if (filter != NULL) {
                int found = 0;
                int i;
                for (i = 0; i < nfilter; i++) {
                    if (filter[i] == inode) {
                        found = 1;
                        if (filter_fds != NULL) {
                            matched_fd = filter_fds[i];
                        }
                        break;
                    }
                }
                if (!found) {
                    continue;
                }
            }
            if (*index >= *capacity) {
                int ncap = (*capacity == 0) ? 64 : (*capacity * 2);
                psutil_net_connection* nc = (psutil_net_connection*)realloc(
                    *conns, (size_t)ncap * sizeof(psutil_net_connection));
                if (nc == NULL) {
                    return -1;
                }
                *conns = nc;
                *capacity = ncap;
            }

            {
                psutil_net_connection* c = &(*conns)[*index];
                memset(c, 0, sizeof(*c));
                c->fd = matched_fd;
                c->family = family;
                c->type = type;
                if (psutil_linux_split_and_format(c->laddr, sizeof(c->laddr),
                                                  lfield, family) != 0) {
                    continue;
                }
                if (type == SOCK_STREAM) {
                    psutil_linux_split_and_format(c->raddr, sizeof(c->raddr),
                                                  rfield, family);
                    c->status = parse_tcp_state((int)state);
                } else {
                    c->raddr[0] = '\0';
                    c->status = PSUTIL_CONN_NONE;
                }
            }
            (*index)++;
        }
    }

    fclose(fp);
    return 0;
}

static psutil_net_connection* psutil_linux_collect_net(const char* kind,
                                                      const unsigned long* filter,
                                                      const int* filter_fds,
                                                      int nfilter,
                                                      int* count) {
    int get_tcp = (kind == NULL || strcmp(kind, "inet") == 0 ||
                   strcmp(kind, "tcp4") == 0 || strcmp(kind, "tcp") == 0 ||
                   strcmp(kind, "tcp6") == 0);
    int get_tcp6 = (kind == NULL || strcmp(kind, "inet") == 0 ||
                    strcmp(kind, "tcp6") == 0 || strcmp(kind, "tcp") == 0);
    int get_udp = (kind == NULL || strcmp(kind, "inet") == 0 ||
                   strcmp(kind, "udp4") == 0 || strcmp(kind, "udp") == 0 ||
                   strcmp(kind, "udp6") == 0);
    int get_udp6 = (kind == NULL || strcmp(kind, "inet") == 0 ||
                    strcmp(kind, "udp6") == 0 || strcmp(kind, "udp") == 0);
    psutil_net_connection* conns = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;

    if (get_tcp && psutil_linux_read_net_file("/proc/net/tcp", AF_INET,
                                              SOCK_STREAM, &conns, &capacity,
                                              &index, filter, filter_fds, nfilter) != 0) {
        goto oom;
    }
    if (get_tcp6 && psutil_linux_read_net_file("/proc/net/tcp6", AF_INET6,
                                               SOCK_STREAM, &conns, &capacity,
                                               &index, filter, filter_fds, nfilter) != 0) {
        goto oom;
    }
    if (get_udp && psutil_linux_read_net_file("/proc/net/udp", AF_INET,
                                              SOCK_DGRAM, &conns, &capacity,
                                              &index, filter, filter_fds, nfilter) != 0) {
        goto oom;
    }
    if (get_udp6 && psutil_linux_read_net_file("/proc/net/udp6", AF_INET6,
                                               SOCK_DGRAM, &conns, &capacity,
                                               &index, filter, filter_fds, nfilter) != 0) {
        goto oom;
    }

    *count = index;
    return conns;

oom:
    free(conns);
    psutil_set_last_error(PSUTIL_ERROR_MEMORY);
    return NULL;
}

psutil_net_connection* psutil_linux_net_connections(const char* kind, int* count) {
    return psutil_linux_collect_net(kind, NULL, NULL, 0, count);
}
static void psutil_linux_format_sockaddr(int family, const struct sockaddr* sa,
                                         char* out, size_t outsz) {
    out[0] = '\0';
    if (sa == NULL) {
        return;
    }
    if (family == AF_INET) {
        const struct sockaddr_in* in = (const struct sockaddr_in*)sa;
        if (inet_ntop(AF_INET, &in->sin_addr, out, (socklen_t)outsz) == NULL) {
            out[0] = '\0';
        }
    } else if (family == AF_INET6) {
        const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)sa;
        if (inet_ntop(AF_INET6, &in6->sin6_addr, out, (socklen_t)outsz) == NULL) {
            out[0] = '\0';
        }
    }
}

static void psutil_linux_format_mac(const struct sockaddr_ll* sll, char* out,
                                    size_t outsz) {
    size_t used = 0;
    int i;

    out[0] = '\0';
    if (sll == NULL) {
        return;
    }
    for (i = 0; i < sll->sll_halen && used + 4 < outsz; i++) {
        used += (size_t)snprintf(out + used, outsz - used, "%s%02x",
                                 (i == 0) ? "" : ":", sll->sll_addr[i]);
    }
}

psutil_net_if_addr* psutil_linux_net_if_addrs(int* count) {
    struct ifaddrs* ifaddr = NULL;
    struct ifaddrs* ifa;
    psutil_net_if_addr* addrs = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;
    if (getifaddrs(&ifaddr) != 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return NULL;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        int family;

        if (ifa->ifa_addr == NULL || ifa->ifa_name == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        // AF_PACKET carries the hardware address; without it callers could
        // not tell interfaces apart by MAC at all.
        if (family != AF_INET && family != AF_INET6 && family != AF_PACKET) {
            continue;
        }

        if (index >= capacity) {
            int ncap = (capacity == 0) ? 32 : (capacity * 2);
            psutil_net_if_addr* na = (psutil_net_if_addr*)realloc(
                addrs, (size_t)ncap * sizeof(psutil_net_if_addr));
            if (na == NULL) {
                free(addrs);
                freeifaddrs(ifaddr);
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            addrs = na;
            capacity = ncap;
        }

        memset(&addrs[index], 0, sizeof(psutil_net_if_addr));
        snprintf(addrs[index].name, sizeof(addrs[index].name), "%s", ifa->ifa_name);
        addrs[index].family = family;

        if (family == AF_INET || family == AF_INET6) {
            psutil_linux_format_sockaddr(family, ifa->ifa_addr,
                                         addrs[index].address,
                                         sizeof(addrs[index].address));
            if (addrs[index].address[0] == '\0') {
                continue;
            }
            if (ifa->ifa_netmask != NULL) {
                psutil_linux_format_sockaddr(family, ifa->ifa_netmask,
                                             addrs[index].netmask,
                                             sizeof(addrs[index].netmask));
            }
            // Point-to-point links keep the peer in ifa_dstaddr, broadcast
            // links keep it in ifa_broadaddr (same union).
            if ((ifa->ifa_flags & IFF_POINTOPOINT) != 0) {
                if (ifa->ifa_dstaddr != NULL) {
                    psutil_linux_format_sockaddr(family, ifa->ifa_dstaddr,
                                                 addrs[index].broadcast,
                                                 sizeof(addrs[index].broadcast));
                }
            } else if ((ifa->ifa_flags & IFF_BROADCAST) != 0 &&
                       ifa->ifa_broadaddr != NULL) {
                psutil_linux_format_sockaddr(family, ifa->ifa_broadaddr,
                                             addrs[index].broadcast,
                                             sizeof(addrs[index].broadcast));
            }
        } else {  // AF_PACKET
            psutil_linux_format_mac((const struct sockaddr_ll*)ifa->ifa_addr,
                                    addrs[index].mac, sizeof(addrs[index].mac));
            if (addrs[index].mac[0] == '\0') {
                continue;
            }
        }
        index++;
    }

    freeifaddrs(ifaddr);
    *count = index;
    return addrs;
}

psutil_net_if_stat* psutil_linux_net_if_stats(int* count) {
    FILE* fp;
    psutil_net_if_stat* stats = NULL;
    int capacity = 0;
    int index = 0;
    int sock;
    char line[512];

    *count = 0;
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return NULL;
    }

    // One socket for every ioctl: the old loop opened and closed one per
    // interface, which is needlessly expensive on many-nic machines.
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    fgets(line, sizeof(line), fp);  // two header lines
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) != NULL) {
        char* colon = strchr(line, ':');
        char name[IFNAMSIZ];
        struct ifreq ifr;

        if (colon == NULL) {
            continue;
        }
        *colon = '\0';
        strncpy(name, line, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        {
            char* p = name;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            memmove(name, p, strlen(p) + 1);
        }
        if (name[0] == '\0') {
            continue;
        }

        if (index >= capacity) {
            int ncap = (capacity == 0) ? 16 : (capacity * 2);
            psutil_net_if_stat* ns = (psutil_net_if_stat*)realloc(
                stats, (size_t)ncap * sizeof(psutil_net_if_stat));
            if (ns == NULL) {
                free(stats);
                fclose(fp);
                if (sock >= 0) {
                    close(sock);
                }
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            stats = ns;
            capacity = ncap;
        }

        memset(&stats[index], 0, sizeof(psutil_net_if_stat));
        snprintf(stats[index].name, sizeof(stats[index].name), "%s", name);
        stats[index].isup = 0;
        stats[index].duplex = PSUTIL_NIC_DUPLEX_UNKNOWN;

        if (sock >= 0) {
            memset(&ifr, 0, sizeof(ifr));
            snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);

            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
                // IFF_UP alone is true for an cable-unplugged NIC; IFF_RUNNING
                // is what actually tells "usable link".
                stats[index].isup =
                    ((ifr.ifr_flags & IFF_UP) != 0 &&
                     (ifr.ifr_flags & IFF_RUNNING) != 0) ? 1 : 0;
            }
            if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
                stats[index].mtu = ifr.ifr_mtu;
            }
        }

        {
            char sys_path[512];
            FILE* sub;

            snprintf(sys_path, sizeof(sys_path), "/sys/class/net/%s/speed", name);
            sub = fopen(sys_path, "r");
            if (sub != NULL) {
                int speed = 0;
                if (fscanf(sub, "%d", &speed) == 1 && speed > 0) {
                    stats[index].speed = speed;
                }
                fclose(sub);
            }

            snprintf(sys_path, sizeof(sys_path), "/sys/class/net/%s/duplex", name);
            sub = fopen(sys_path, "r");
            if (sub != NULL) {
                char duplex[32];
                if (fscanf(sub, "%31s", duplex) == 1) {
                    if (strcmp(duplex, "full") == 0) {
                        stats[index].duplex = PSUTIL_NIC_DUPLEX_FULL;
                    } else if (strcmp(duplex, "half") == 0) {
                        stats[index].duplex = PSUTIL_NIC_DUPLEX_HALF;
                    }
                }
                fclose(sub);
            }
        }

        index++;
    }

    if (sock >= 0) {
        close(sock);
    }
    fclose(fp);
    *count = index;
    return stats;
}

// Sector counts in /proc/diskstats are in 512-byte units, but the device
// itself may use 4096-byte sectors; the sysfs size is the real byte size.
static unsigned long long psutil_linux_sector_size(const char* dev) {
    char path[320];
    FILE* fp;
    unsigned long long size = 0;

    snprintf(path, sizeof(path), "/sys/block/%s/queue/logical_block_size", dev);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 512;
    }
    if (fscanf(fp, "%llu", &size) != 1 || size == 0) {
        size = 512;
    }
    fclose(fp);
    return size;
}

psutil_io_counters psutil_linux_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};
    FILE* fp;

    (void)perdisk;
    fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return counters;
    }

    {
        char line[512];
        while (fgets(line, sizeof(line), fp) != NULL) {
            unsigned int major = 0, minor = 0;
            char device[64];
            unsigned long long reads = 0, reads_merged = 0, sectors_read = 0, read_time = 0;
            unsigned long long writes = 0, writes_merged = 0, sectors_written = 0, write_time = 0;
            char sys_block[300];

            // major minor name reads reads_merged sectors_read read_time
            // writes writes_merged sectors_written write_time ...
            if (sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                       &major, &minor, device, &reads, &reads_merged,
                       &sectors_read, &read_time, &writes, &writes_merged,
                       &sectors_written, &write_time) < 10) {
                continue;
            }
            (void)major;
            (void)minor;

            if (strncmp(device, "loop", 4) == 0 ||
                strncmp(device, "ram", 3) == 0 ||
                strncmp(device, "zram", 4) == 0 ||
                strncmp(device, "fd", 2) == 0) {
                continue;
            }

            // /proc/diskstats lists whole disks *and* their partitions.
            // Summing both double counts every byte, so keep only entries
            // that have a /sys/block directory, i.e. real block devices.
            snprintf(sys_block, sizeof(sys_block), "/sys/block/%s", device);
            if (access(sys_block, F_OK) != 0) {
                continue;
            }

            counters.read_count += reads;
            counters.write_count += writes;
            {
                unsigned long long sector = psutil_linux_sector_size(device);
                counters.read_bytes += sectors_read * sector;
                counters.write_bytes += sectors_written * sector;
            }
            counters.read_time += read_time;             // milliseconds
            counters.write_time += write_time;
        }
    }

    fclose(fp);
    return counters;
}

// Pseudo filesystems are always skipped unless "all" was requested.
static int psutil_linux_is_pseudo_fs(const char* fstype) {
    static const char* pseudo[] = {
        "autofs", "binfmt_misc", "bpf", "cgroup", "cgroup2", "configfs",
        "debugfs", "devpts", "devtmpfs", "fusectl", "hugetlbfs", "mqueue",
        "nsfs", "proc", "pstore", "rootfs", "securityfs", "selinuxfs",
        "sysfs", "tmpfs", "tracefs", NULL
    };
    int i;

    for (i = 0; pseudo[i] != NULL; i++) {
        if (strcmp(fstype, pseudo[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Decodes /proc/mounts escaping (\040 for space, \011 tab, \134 backslash).
static void psutil_linux_unescape_mount(const char* in, char* out, size_t outsz) {
    size_t used = 0;

    while (*in != '\0' && used + 1 < outsz) {
        if (*in == '\\' && in[1] >= '0' && in[1] <= '7' &&
            in[2] >= '0' && in[2] <= '7' && in[3] >= '0' && in[3] <= '7') {
            out[used++] = (char)(((in[1] - '0') << 6) |
                                 ((in[2] - '0') << 3) |
                                 (in[3] - '0'));
            in += 4;
        } else {
            out[used++] = *in++;
        }
    }
    out[used] = '\0';
}

psutil_disk_partition* psutil_linux_disk_partitions(int all, int* count) {
    FILE* fp;
    psutil_disk_partition* partitions = NULL;
    int capacity = 0;
    int index = 0;
    char line[1024];

    *count = 0;
    fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        fp = fopen("/etc/mtab", "r");
        if (fp == NULL) {
            psutil_set_last_error(PSUTIL_ERROR_IO);
            return NULL;
        }
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char device[256], mountpoint[256], fstype[64], opts[256];
        int keep = 1;

        if (sscanf(line, "%255s %255s %63s %255s", device, mountpoint, fstype,
                   opts) != 4) {
            continue;
        }

        if (!all) {
            if (psutil_linux_is_pseudo_fs(fstype)) {
                keep = 0;
            } else if (device[0] != '/' &&
                       strcmp(fstype, "nfs") != 0 &&
                       strcmp(fstype, "nfs4") != 0 &&
                       strcmp(fstype, "cifs") != 0 &&
                       strcmp(fstype, "smb3") != 0 &&
                       strcmp(fstype, "zfs") != 0 &&
                       strcmp(fstype, "overlay") != 0 &&
                       strncmp(fstype, "fuse", 4) != 0) {
                keep = 0;
            }
        }
        if (!keep) {
            continue;
        }

        if (index >= capacity) {
            int ncap = (capacity == 0) ? 32 : (capacity * 2);
            psutil_disk_partition* np = (psutil_disk_partition*)realloc(
                partitions, (size_t)ncap * sizeof(psutil_disk_partition));
            if (np == NULL) {
                free(partitions);
                fclose(fp);
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            partitions = np;
            capacity = ncap;
        }

        memset(&partitions[index], 0, sizeof(psutil_disk_partition));
        snprintf(partitions[index].device, sizeof(partitions[index].device),
                 "%s", device);
        psutil_linux_unescape_mount(mountpoint, partitions[index].mountpoint,
                                    sizeof(partitions[index].mountpoint));
        snprintf(partitions[index].fstype, sizeof(partitions[index].fstype),
                 "%s", fstype);
        snprintf(partitions[index].opts, sizeof(partitions[index].opts),
                 "%s", opts);
        index++;
    }

    fclose(fp);
    *count = index;
    return partitions;
}

psutil_disk_usage psutil_linux_disk_usage(const char* path) {
    psutil_disk_usage usage = {0};
    struct statvfs svfs;

    if (path == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return usage;
    }
    // statvfs (not statfs): f_frsize is the real fragment size, f_bsize is
    // only the preferred I/O size, and the two differ on many filesystems.
    if (statvfs(path, &svfs) != 0 || svfs.f_frsize == 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return usage;
    }

    usage.total = (uint64_t)svfs.f_blocks * svfs.f_frsize;
    usage.free = (uint64_t)svfs.f_bavail * svfs.f_frsize;   // unprivileged view
    usage.used = ((uint64_t)svfs.f_blocks - (uint64_t)svfs.f_bfree) * svfs.f_frsize;
    if (usage.total > 0) {
        usage.percent = (double)usage.used / (double)(usage.used + usage.free) * 100.0;
        if (usage.percent > 100.0) {
            usage.percent = 100.0;
        }
    }
    return usage;
}

psutil_user* psutil_linux_users(int* count) {
    struct utmp* ut;
    psutil_user* users = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;
    setutent();

    while ((ut = getutent()) != NULL) {
        size_t len;

        if (ut->ut_type != USER_PROCESS || ut->ut_user[0] == '\0') {
            continue;
        }

        if (index >= capacity) {
            int ncap = (capacity == 0) ? 16 : (capacity * 2);
            psutil_user* nu = (psutil_user*)realloc(
                users, (size_t)ncap * sizeof(psutil_user));
            if (nu == NULL) {
                free(users);
                endutent();
                psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                return NULL;
            }
            users = nu;
            capacity = ncap;
        }

        memset(&users[index], 0, sizeof(psutil_user));

        // ut_user is UT_NAMESIZE (32) bytes and *not* NUL terminated when the
        // name fills it; copying sizeof(dest) - 1 bytes read past the field
        // into ut_id/ut_line and produced garbage names.
        len = strnlen(ut->ut_user, sizeof(ut->ut_user));
        if (len >= sizeof(users[index].name)) {
            len = sizeof(users[index].name) - 1;
        }
        memcpy(users[index].name, ut->ut_user, len);

        len = strnlen(ut->ut_line, sizeof(ut->ut_line));
        if (len >= sizeof(users[index].terminal)) {
            len = sizeof(users[index].terminal) - 1;
        }
        memcpy(users[index].terminal, ut->ut_line, len);

        len = strnlen(ut->ut_host, sizeof(ut->ut_host));
        if (len >= sizeof(users[index].host)) {
            len = sizeof(users[index].host) - 1;
        }
        memcpy(users[index].host, ut->ut_host, len);

        users[index].started = (double)ut->ut_tv.tv_sec;
        index++;
    }

    endutent();
    *count = index;

    if (index == 0) {
        free(users);
        return NULL;
    }
    return users;
}

double psutil_linux_boot_time(void) {
    // Same cached btime value the process layer uses for create_time, so
    // uptime-based drift cannot make the two disagree.
    return psutil_linux_boot_time_cached();
}


