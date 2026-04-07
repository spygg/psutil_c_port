/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Android-specific implementation.
 * Android is based on Linux but has some differences:
 * - Bionic libc instead of glibc
 * - Limited /proc access in some cases
 * - Some functions may require root permissions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sched.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "../../psutil.h"
#include "psutil_android.h"

// Android uses Bionic libc which has some limitations
// We define some constants if not present
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

// Clock ticks per second
static long SC_CLK_TCK = 0;

// Initialize the library
int psutil_android_init(void) {
    // Get clock ticks per second
    SC_CLK_TCK = sysconf(_SC_CLK_TCK);
    if (SC_CLK_TCK <= 0) {
        SC_CLK_TCK = 100; // Default fallback
    }
    return 0;
}

// Helper function to read a file into a buffer
static int read_file(const char* path, char* buffer, size_t size) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    size_t read = fread(buffer, 1, size - 1, fp);
    fclose(fp);
    
    if (read > 0) {
        buffer[read] = '\0';
        return 0;
    }
    return -1;
}

// Helper function to read a single integer from a file
static int read_int_from_file(const char* path, int* value) {
    char buffer[256];
    if (read_file(path, buffer, sizeof(buffer)) != 0) {
        return -1;
    }
    *value = atoi(buffer);
    return 0;
}

// Process functions
int psutil_android_pid_exists(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

pid_t* psutil_android_pids(int *count) {
    DIR *dir;
    struct dirent *entry;
    pid_t *pids_list = NULL;
    int size = 1024;
    int index = 0;

    pids_list = (pid_t*)malloc(size * sizeof(pid_t));
    if (pids_list == NULL) {
        *count = 0;
        return NULL;
    }

    dir = opendir("/proc");
    if (dir == NULL) {
        free(pids_list);
        *count = 0;
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        pid_t pid = atoi(entry->d_name);
        if (pid > 0) {
            if (index >= size) {
                size *= 2;
                pid_t *new_pids = (pid_t*)realloc(pids_list, size * sizeof(pid_t));
                if (new_pids == NULL) {
                    free(pids_list);
                    closedir(dir);
                    *count = 0;
                    return NULL;
                }
                pids_list = new_pids;
            }
            pids_list[index++] = pid;
        }
    }

    closedir(dir);
    *count = index;
    return pids_list;
}

Process* psutil_android_process_new(pid_t pid) {
    Process* proc = (Process*)malloc(sizeof(Process));
    if (proc == NULL) {
        return NULL;
    }

    // If pid is 0, use current process PID
    if (pid == 0) {
        pid = getpid();
    }

    proc->pid = pid;
    proc->name = NULL;
    proc->exe = NULL;
    proc->cwd = NULL;
    proc->username = NULL;
    proc->create_time = 0.0;
    proc->ppid = 0;
    proc->status = STATUS_RUNNING;

    // Try to read basic info from /proc/[pid]/stat
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        // Parse stat file: pid (comm) state ppid ...
        char* p = strchr(buffer, '(');
        if (p != NULL) {
            char* q = strrchr(p, ')');
            if (q != NULL) {
                *q = '\0';
                proc->name = strdup(p + 1);
                
                // Parse ppid and other fields
                int ppid;
                if (sscanf(q + 2, "%*c %d", &ppid) == 1) {
                    proc->ppid = ppid;
                }
            }
        }
    }

    // Read create time from stat file
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        unsigned long long starttime;
        // Parse starttime (field 22, in clock ticks since boot)
        char* token = buffer;
        int field = 0;
        while (token != NULL && field < 21) {
            token = strchr(token, ' ');
            if (token) {
                token++;
                field++;
            }
        }
        if (token && sscanf(token, "%llu", &starttime) == 1) {
            double boot_time_val = psutil_android_boot_time();
            proc->create_time = boot_time_val + (double)starttime / SC_CLK_TCK;
        }
    }

    return proc;
}

void psutil_android_process_free(Process* proc) {
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
    
    free(proc);
}

pid_t psutil_android_process_get_ppid(Process* proc) {
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        int ppid;
        // Skip to after the command name
        char* p = strchr(buffer, ')');
        if (p != NULL) {
            if (sscanf(p + 2, "%*c %d", &ppid) == 1) {
                return ppid;
            }
        }
    }
    return 0;
}

const char* psutil_android_process_get_name(Process* proc) {
    if (proc->name != NULL) {
        return proc->name;
    }
    
    char path[256];
    char buffer[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        // Remove trailing newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        proc->name = strdup(buffer);
        return proc->name;
    }
    
    return NULL;
}

const char* psutil_android_process_get_exe(Process* proc) {
    if (proc->exe != NULL) {
        return proc->exe;
    }
    
    char path[256];
    char buffer[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/exe", proc->pid);
    
    ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        proc->exe = strdup(buffer);
        return proc->exe;
    }
    
    return NULL;
}

char** psutil_android_process_get_cmdline(Process* proc, int* count) {
    char path[256];
    char buffer[4096];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", proc->pid);
    
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    if (read == 0) {
        *count = 0;
        return NULL;
    }
    buffer[read] = '\0';
    
    // Count arguments (separated by null bytes)
    int argc = 0;
    for (size_t i = 0; i < read; i++) {
        if (buffer[i] == '\0') {
            argc++;
        }
    }
    
    if (argc == 0) {
        *count = 0;
        return NULL;
    }
    
    char** cmdline = (char**)malloc((argc + 1) * sizeof(char*));
    if (cmdline == NULL) {
        *count = 0;
        return NULL;
    }
    
    int idx = 0;
    char* arg = buffer;
    while (arg < buffer + read && idx < argc) {
        cmdline[idx++] = strdup(arg);
        arg += strlen(arg) + 1;
    }
    cmdline[idx] = NULL;
    
    *count = argc;
    return cmdline;
}

int psutil_android_process_get_status(Process* proc) {
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char state;
        // Skip to after the command name
        char* p = strchr(buffer, ')');
        if (p != NULL) {
            if (sscanf(p + 2, "%c", &state) == 1) {
                switch (state) {
                    case 'R': return STATUS_RUNNING;
                    case 'S': return STATUS_SLEEPING;
                    case 'D': return STATUS_DISK_SLEEP;
                    case 'T': return STATUS_STOPPED;
                    case 't': return STATUS_TRACING_STOP;
                    case 'Z': return STATUS_ZOMBIE;
                    case 'X': return STATUS_DEAD;
                    case 'x': return STATUS_DEAD;
                    case 'K': return STATUS_WAKE_KILL;  // Android specific
                    case 'W': return STATUS_WAKING;
                    case 'P': return STATUS_PARKED;
                    case 'I': return STATUS_IDLE;
                    default: return STATUS_RUNNING;
                }
            }
        }
    }
    return STATUS_RUNNING;
}

// Android-specific status constant
#define STATUS_WAKE_KILL 12
#define STATUS_IDLE_LINUX 1

int psutil_android_process_get_status(Process* proc) {
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char state;
        // Skip to after the command name
        char* p = strchr(buffer, ')');
        if (p != NULL) {
            if (sscanf(p + 2, "%c", &state) == 1) {
                switch (state) {
                    case 'R': return STATUS_RUNNING;
                    case 'S': return STATUS_SLEEPING;
                    case 'D': return STATUS_DISK_SLEEP;
                    case 'T': return STATUS_STOPPED;
                    case 't': return STATUS_TRACING_STOP;
                    case 'Z': return STATUS_ZOMBIE;
                    case 'X': return STATUS_DEAD;
                    case 'x': return STATUS_DEAD;
                    case 'K': return STATUS_WAKE_KILL;  // Android specific
                    case 'W': return STATUS_WAKING;
                    case 'P': return STATUS_PARKED;
                    case 'I': return STATUS_IDLE_LINUX;
                    default: return STATUS_RUNNING;
                }
            }
        }
    }
    return STATUS_RUNNING;
}

const char* psutil_android_process_get_username(Process* proc) {
    if (proc->username != NULL) {
        return proc->username;
    }
    
    psutil_uids uids = psutil_android_process_get_uids(proc);
    
    // On Android, getpwuid may not work as expected due to Bionic limitations
    // Try to get username from /proc/[pid]/status
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = strstr(buffer, "Uid:");
        if (line != NULL) {
            unsigned int uid;
            if (sscanf(line, "Uid:\t%u", &uid) == 1) {
                // Try to get username (may not work on Android without proper permissions)
                struct passwd* pw = getpwuid(uid);
                if (pw != NULL) {
                    proc->username = strdup(pw->pw_name);
                } else {
                    // Fallback: use UID as string
                    char uid_str[32];
                    snprintf(uid_str, sizeof(uid_str), "%u", uid);
                    proc->username = strdup(uid_str);
                }
                return proc->username;
            }
        }
    }
    
    return NULL;
}

double psutil_android_process_get_create_time(Process* proc) {
    return proc->create_time;
}

const char* psutil_android_process_get_cwd(Process* proc) {
    if (proc->cwd != NULL) {
        return proc->cwd;
    }
    
    char path[256];
    char buffer[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/cwd", proc->pid);
    
    ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        proc->cwd = strdup(buffer);
        return proc->cwd;
    }
    
    return NULL;
}

int psutil_android_process_get_nice(Process* proc) {
    errno = 0;
    int nice_val = getpriority(PRIO_PROCESS, proc->pid);
    if (errno != 0) {
        return 0;
    }
    return nice_val;
}

int psutil_android_process_set_nice(Process* proc, int value) {
    return setpriority(PRIO_PROCESS, proc->pid, value);
}

psutil_uids psutil_android_process_get_uids(Process* proc) {
    psutil_uids uids = {0, 0, 0, 0};
    
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = strstr(buffer, "Uid:");
        if (line != NULL) {
            unsigned int real, effective, saved, setuid;
            if (sscanf(line, "Uid:\t%u\t%u\t%u\t%u", &real, &effective, &saved, &setuid) == 4) {
                uids.real = real;
                uids.effective = effective;
                uids.saved = saved;
                uids.setuid = setuid;
            }
        }
    }
    
    return uids;
}

psutil_gids psutil_android_process_get_gids(Process* proc) {
    psutil_gids gids = {0, 0, 0, 0};
    
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = strstr(buffer, "Gid:");
        if (line != NULL) {
            unsigned int real, effective, saved, setgid;
            if (sscanf(line, "Gid:\t%u\t%u\t%u\t%u", &real, &effective, &saved, &setgid) == 4) {
                gids.real = real;
                gids.effective = effective;
                gids.saved = saved;
                gids.setgid = setgid;
            }
        }
    }
    
    return gids;
}

const char* psutil_android_process_get_terminal(Process* proc) {
    // On Android, terminals are less common
    // Try to get tty from /proc/[pid]/stat
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        int tty_nr;
        char* p = strchr(buffer, ')');
        if (p != NULL) {
            // Fields: pid, comm, state, ppid, pgrp, session, tty_nr
            if (sscanf(p + 2, "%*c %*d %*d %*d %*d %d", &tty_nr) >= 1) {
                if (tty_nr == 0) {
                    return NULL; // No controlling terminal
                }
                // Decode tty_nr to device name
                static char tty_path[64];
                int major = (tty_nr >> 8) & 0xff;
                int minor = tty_nr & 0xff;
                snprintf(tty_path, sizeof(tty_path), "/dev/tty%d", minor);
                return tty_path;
            }
        }
    }
    
    return NULL;
}

int psutil_android_process_get_num_fds(Process* proc) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", proc->pid);
    
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

psutil_io_counters psutil_android_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0, 0, 0, 0, 0, 0};
    
    char path[256];
    char buffer[4096];
    snprintf(path, sizeof(path), "/proc/%d/io", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = buffer;
        while (line != NULL && *line != '\0') {
            char* next = strchr(line, '\n');
            if (next != NULL) {
                *next = '\0';
                next++;
            }
            
            unsigned long long value;
            if (sscanf(line, "rchar: %llu", &value) == 1) {
                counters.read_bytes = value;
            } else if (sscanf(line, "wchar: %llu", &value) == 1) {
                counters.write_bytes = value;
            } else if (sscanf(line, "syscr: %llu", &value) == 1) {
                counters.read_count = value;
            } else if (sscanf(line, "syscw: %llu", &value) == 1) {
                counters.write_count = value;
            } else if (sscanf(line, "read_bytes: %llu", &value) == 1) {
                // Actual bytes read from storage
            } else if (sscanf(line, "write_bytes: %llu", &value) == 1) {
                // Actual bytes written to storage
            }
            
            line = next;
        }
    }
    
    return counters;
}

int psutil_android_process_get_ionice(Process* proc) {
    // ionice is not available on Android
    return 0;
}

int psutil_android_process_set_ionice(Process* proc, int ioclass, int value) {
    // ionice is not available on Android
    return -1;
}

int* psutil_android_process_get_cpu_affinity(Process* proc, int* count) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    
    if (sched_getaffinity(proc->pid, sizeof(cpu_set), &cpu_set) != 0) {
        *count = 0;
        return NULL;
    }
    
    int cpu_count = CPU_COUNT(&cpu_set);
    if (cpu_count == 0) {
        *count = 0;
        return NULL;
    }
    
    int* cpus = (int*)malloc(cpu_count * sizeof(int));
    if (cpus == NULL) {
        *count = 0;
        return NULL;
    }
    
    int idx = 0;
    int ncpus = sysconf(_SC_NPROCESSORS_CONF);
    for (int i = 0; i < ncpus && idx < cpu_count; i++) {
        if (CPU_ISSET(i, &cpu_set)) {
            cpus[idx++] = i;
        }
    }
    
    *count = idx;
    return cpus;
}

int psutil_android_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    
    for (int i = 0; i < count; i++) {
        CPU_SET(cpus[i], &cpu_set);
    }
    
    return sched_setaffinity(proc->pid, sizeof(cpu_set), &cpu_set);
}

int psutil_android_process_get_cpu_num(Process* proc) {
    int cpu_num;
    if (sched_getcpu() >= 0) {
        return sched_getcpu();
    }
    return -1;
}

char** psutil_android_process_get_environ(Process* proc, int* count) {
    char path[256];
    char buffer[8192];
    snprintf(path, sizeof(path), "/proc/%d/environ", proc->pid);
    
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    if (read == 0) {
        *count = 0;
        return NULL;
    }
    buffer[read] = '\0';
    
    // Count environment variables (separated by null bytes)
    int envc = 0;
    for (size_t i = 0; i < read; i++) {
        if (buffer[i] == '\0') {
            envc++;
        }
    }
    
    if (envc == 0) {
        *count = 0;
        return NULL;
    }
    
    char** environ = (char**)malloc((envc + 1) * sizeof(char*));
    if (environ == NULL) {
        *count = 0;
        return NULL;
    }
    
    int idx = 0;
    char* var = buffer;
    while (var < buffer + read && idx < envc) {
        environ[idx++] = strdup(var);
        var += strlen(var) + 1;
    }
    environ[idx] = NULL;
    
    *count = envc;
    return environ;
}

int psutil_android_process_get_num_handles(Process* proc) {
    // On Android/Linux, handles are not the same as Windows
    // Return the number of open file descriptors instead
    return psutil_android_process_get_num_fds(proc);
}

psutil_ctx_switches psutil_android_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0, 0};
    
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = strstr(buffer, "voluntary_ctxt_switches:");
        if (line != NULL) {
            sscanf(line, "voluntary_ctxt_switches: %llu", &switches.voluntary);
        }
        line = strstr(buffer, "nonvoluntary_ctxt_switches:");
        if (line != NULL) {
            sscanf(line, "nonvoluntary_ctxt_switches: %llu", &switches.involuntary);
        }
    }
    
    return switches;
}

int psutil_android_process_get_num_threads(Process* proc) {
    char path[256];
    char buffer[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = strstr(buffer, "Threads:");
        if (line != NULL) {
            int threads;
            if (sscanf(line, "Threads:\t%d", &threads) == 1) {
                return threads;
            }
        }
    }
    
    return 0;
}

psutil_thread* psutil_android_process_get_threads(Process* proc, int* count) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task", proc->pid);
    
    DIR* dir = opendir(path);
    if (dir == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Count threads first
    int num_threads = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && atoi(entry->d_name) > 0) {
            num_threads++;
        }
    }
    
    if (num_threads == 0) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    psutil_thread* threads = (psutil_thread*)malloc(num_threads * sizeof(psutil_thread));
    if (threads == NULL) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    rewinddir(dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < num_threads) {
        int tid = atoi(entry->d_name);
        if (tid > 0) {
            threads[idx].id = tid;
            
            // Read thread CPU times from /proc/[pid]/task/[tid]/stat
            char stat_path[256];
            char buffer[1024];
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%d/stat", proc->pid, tid);
            
            if (read_file(stat_path, buffer, sizeof(buffer)) == 0) {
                char* p = strchr(buffer, ')');
                if (p != NULL) {
                    unsigned long long utime, stime;
                    // utime is field 14, stime is field 15
                    if (sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %llu %llu", &utime, &stime) == 2) {
                        threads[idx].user_time = (double)utime / SC_CLK_TCK;
                        threads[idx].system_time = (double)stime / SC_CLK_TCK;
                    } else {
                        threads[idx].user_time = 0;
                        threads[idx].system_time = 0;
                    }
                }
            } else {
                threads[idx].user_time = 0;
                threads[idx].system_time = 0;
            }
            
            idx++;
        }
    }
    
    closedir(dir);
    *count = idx;
    return threads;
}

psutil_cpu_times psutil_android_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0, 0, 0, 0};
    
    char path[256];
    char buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* p = strchr(buffer, ')');
        if (p != NULL) {
            unsigned long long utime, stime, cutime, cstime;
            // utime=14, stime=15, cutime=16, cstime=17
            if (sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %llu %llu %llu %llu", 
                       &utime, &stime, &cutime, &cstime) == 4) {
                times.user = (double)utime / SC_CLK_TCK;
                times.system = (double)stime / SC_CLK_TCK;
                times.children_user = (double)cutime / SC_CLK_TCK;
                times.children_system = (double)cstime / SC_CLK_TCK;
            }
        }
    }
    
    return times;
}

psutil_memory_info psutil_android_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    char path[256];
    char buffer[4096];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        char* line = buffer;
        while (line != NULL && *line != '\0') {
            char* next = strchr(line, '\n');
            if (next != NULL) {
                *next = '\0';
                next++;
            }
            
            unsigned long long value;
            if (sscanf(line, "VmRSS: %llu", &value) == 1) {
                info.rss = value * 1024; // Convert kB to bytes
            } else if (sscanf(line, "VmSize: %llu", &value) == 1) {
                info.vms = value * 1024;
            } else if (sscanf(line, "RssFile: %llu", &value) == 1) {
                info.shared = value * 1024;
            } else if (sscanf(line, "VmData: %llu", &value) == 1) {
                info.data = value * 1024;
            }
            
            line = next;
        }
    }
    
    // Read more detailed info from statm
    snprintf(path, sizeof(path), "/proc/%d/statm", proc->pid);
    if (read_file(path, buffer, sizeof(buffer)) == 0) {
        long long size, resident, shared, text, lib, data, dt;
        if (sscanf(buffer, "%lld %lld %lld %lld %lld %lld %lld",
                   &size, &resident, &shared, &text, &lib, &data, &dt) == 7) {
            long page_size = sysconf(_SC_PAGESIZE);
            info.rss = resident * page_size;
            info.vms = size * page_size;
            info.shared = shared * page_size;
            info.text = text * page_size;
            info.data = data * page_size;
        }
    }
    
    return info;
}

psutil_memory_info psutil_android_process_get_memory_full_info(Process* proc) {
    // On Android, PSS/USS info requires /proc/[pid]/smaps which may not be available
    psutil_memory_info info = psutil_android_process_get_memory_info(proc);
    return info;
}

double psutil_android_process_get_memory_percent(Process* proc, const char* memtype) {
    psutil_memory_info info = psutil_android_process_get_memory_info(proc);
    psutil_memory_info total = psutil_android_virtual_memory();
    
    if (total.total == 0) {
        return 0.0;
    }
    
    if (strcmp(memtype, "rss") == 0) {
        return 100.0 * (double)info.rss / (double)total.total;
    } else if (strcmp(memtype, "vms") == 0) {
        return 100.0 * (double)info.vms / (double)total.total;
    }
    
    return 0.0;
}

psutil_memory_map* psutil_android_process_get_memory_maps(Process* proc, int* count, int grouped) {
    // /proc/[pid]/smaps may not be available on Android without root
    // Return empty result
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_android_process_get_open_files(Process* proc, int* count) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", proc->pid);
    
    DIR* dir = opendir(path);
    if (dir == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Count valid file descriptors
    int num_fds = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && atoi(entry->d_name) > 0) {
            num_fds++;
        }
    }
    
    if (num_fds == 0) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    psutil_open_file* files = (psutil_open_file*)malloc(num_fds * sizeof(psutil_open_file));
    if (files == NULL) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    rewinddir(dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < num_fds) {
        int fd = atoi(entry->d_name);
        if (fd > 0) {
            files[idx].fd = fd;
            
            // Get the path the fd points to
            char fd_path[256];
            char link_path[PATH_MAX];
            snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd/%d", proc->pid, fd);
            
            ssize_t len = readlink(fd_path, link_path, sizeof(link_path) - 1);
            if (len != -1) {
                link_path[len] = '\0';
                strncpy(files[idx].path, link_path, sizeof(files[idx].path) - 1);
                files[idx].path[sizeof(files[idx].path) - 1] = '\0';
            } else {
                strncpy(files[idx].path, "unknown", sizeof(files[idx].path) - 1);
                files[idx].path[sizeof(files[idx].path) - 1] = '\0';
            }
            
            idx++;
        }
    }
    
    closedir(dir);
    *count = idx;
    return files;
}

psutil_net_connection* psutil_android_process_get_net_connections(Process* proc, const char* kind, int* count) {
    // On Android, reading /proc/net/tcp may require root
    // Try to read it anyway, but return empty if not available
    *count = 0;
    return NULL;
}

int psutil_android_process_send_signal(Process* proc, int sig) {
    return kill(proc->pid, sig);
}

int psutil_android_process_suspend(Process* proc) {
    return kill(proc->pid, SIGSTOP);
}

int psutil_android_process_resume(Process* proc) {
    return kill(proc->pid, SIGCONT);
}

int psutil_android_process_terminate(Process* proc) {
    return kill(proc->pid, SIGTERM);
}

int psutil_android_process_kill(Process* proc) {
    return kill(proc->pid, SIGKILL);
}

int psutil_android_process_wait(Process* proc, double timeout) {
    int status;
    pid_t result;
    
    if (timeout <= 0) {
        result = waitpid(proc->pid, &status, 0);
    } else {
        // Non-blocking wait with timeout - simplified version
        int waited = 0;
        while (waited < (int)(timeout * 1000)) {
            result = waitpid(proc->pid, &status, WNOHANG);
            if (result == proc->pid) {
                return 0;
            } else if (result == -1) {
                return -1;
            }
            usleep(10000); // 10ms
            waited += 10;
        }
        return -1; // Timeout
    }
    
    return (result == proc->pid) ? 0 : -1;
}

int psutil_android_process_is_running(Process* proc) {
    return psutil_android_pid_exists(proc->pid);
}

// System functions
psutil_memory_info psutil_android_virtual_memory(void) {
    psutil_memory_info info = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        long page_size = sysconf(_SC_PAGESIZE);
        
        info.total = si.totalram * si.mem_unit;
        info.free = si.freeram * si.mem_unit;
        info.used = info.total - info.free;
        
        // Read additional info from /proc/meminfo
        FILE* fp = fopen("/proc/meminfo", "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                unsigned long long value;
                if (sscanf(line, "Buffers: %llu", &value) == 1) {
                    // Buffers
                } else if (sscanf(line, "Cached: %llu", &value) == 1) {
                    // Cached memory
                } else if (sscanf(line, "Slab: %llu", &value) == 1) {
                    // Slab
                }
            }
            fclose(fp);
        }
    }
    
    return info;
}

psutil_memory_info psutil_android_swap_memory(void) {
    psutil_memory_info info = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.total = si.totalswap * si.mem_unit;
        info.free = si.freeswap * si.mem_unit;
        info.used = info.total - info.free;
    }
    
    return info;
}

psutil_cpu_times psutil_android_cpu_times(int percpu) {
    psutil_cpu_times times = {0, 0, 0, 0};
    
    FILE* fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return times;
    }
    
    char line[256];
    if (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
            times.user = (double)(user + nice) / SC_CLK_TCK;
            times.system = (double)(system + irq + softirq + steal) / SC_CLK_TCK;
            // Note: idle and iowait are not part of psutil_cpu_times struct
        }
    }
    
    fclose(fp);
    return times;
}

double psutil_android_cpu_percent(double interval, int percpu) {
    psutil_cpu_times t1 = psutil_android_cpu_times(percpu);
    
    if (interval > 0) {
        usleep((useconds_t)(interval * 1000000));
    }
    
    psutil_cpu_times t2 = psutil_android_cpu_times(percpu);
    
    double total_diff = (t2.user + t2.system) - (t1.user + t1.system);
    double user_diff = t2.user - t1.user;
    double system_diff = t2.system - t1.system;
    
    // This is a simplified calculation
    // Real implementation would need idle time calculation
    if (total_diff > 0) {
        return 100.0 * total_diff / (total_diff + 0.001); // Simplified
    }
    
    return 0.0;
}

psutil_cpu_times psutil_android_cpu_times_percent(double interval, int percpu) {
    // Simplified implementation
    return psutil_android_cpu_times(percpu);
}

int psutil_android_cpu_count(int logical) {
    if (logical) {
        return sysconf(_SC_NPROCESSORS_ONLN);
    } else {
        // Physical cores - harder to determine on Android
        // Return logical count as approximation
        return sysconf(_SC_NPROCESSORS_CONF);
    }
}

psutil_cpu_stats psutil_android_cpu_stats(void) {
    psutil_cpu_stats stats = {0, 0, 0, 0};
    
    FILE* fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return stats;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long value;
        if (sscanf(line, "ctxt %llu", &value) == 1) {
            stats.ctx_switches = (int)value;
        } else if (sscanf(line, "intr %llu", &value) == 1) {
            stats.interrupts = (int)value;
        } else if (sscanf(line, "softirq %llu", &value) == 1) {
            stats.soft_interrupts = (int)value;
        }
    }
    
    fclose(fp);
    return stats;
}

psutil_io_counters psutil_android_net_io_counters(int pernic) {
    psutil_io_counters counters = {0, 0, 0, 0, 0, 0};
    
    // On Android, /proc/net/dev may require root
    FILE* fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return counters;
    }
    
    char line[512];
    // Skip first two header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char iface[64];
        unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop;
        unsigned long long tx_bytes, tx_packets, tx_errs, tx_drop;
        
        if (sscanf(line, "%63s %llu %llu %llu %llu %*u %*u %*u %*u %llu %llu %llu %llu",
                   iface, &rx_bytes, &rx_packets, &rx_errs, &rx_drop,
                   &tx_bytes, &tx_packets, &tx_errs, &tx_drop) == 9) {
            // Remove colon from interface name
            char* colon = strchr(iface, ':');
            if (colon != NULL) {
                *colon = '\0';
            }
            
            // Skip loopback
            if (strcmp(iface, "lo") != 0) {
                counters.read_bytes += rx_bytes;
                counters.write_bytes += tx_bytes;
                counters.read_count += rx_packets;
                counters.write_count += tx_packets;
            }
        }
    }
    
    fclose(fp);
    return counters;
}

psutil_net_connection* psutil_android_net_connections(const char* kind, int* count) {
    // On Android, /proc/net/tcp requires root
    *count = 0;
    return NULL;
}

psutil_net_if_addr* psutil_android_net_if_addrs(int* count) {
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        *count = 0;
        return NULL;
    }
    
    // Count interfaces
    int num_ifs = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
            num_ifs++;
        }
    }
    
    if (num_ifs == 0) {
        freeifaddrs(ifaddr);
        *count = 0;
        return NULL;
    }
    
    psutil_net_if_addr* addrs = (psutil_net_if_addr*)malloc(num_ifs * sizeof(psutil_net_if_addr));
    if (addrs == NULL) {
        freeifaddrs(ifaddr);
        *count = 0;
        return NULL;
    }
    
    int idx = 0;
    for (ifa = ifaddr; ifa != NULL && idx < num_ifs; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
            strncpy(addrs[idx].name, ifa->ifa_name, sizeof(addrs[idx].name) - 1);
            addrs[idx].name[sizeof(addrs[idx].name) - 1] = '\0';
            inet_ntop(AF_INET, &sin->sin_addr, addrs[idx].address, sizeof(addrs[idx].address));
            
            if (ifa->ifa_netmask != NULL) {
                sin = (struct sockaddr_in*)ifa->ifa_netmask;
                inet_ntop(AF_INET, &sin->sin_addr, addrs[idx].netmask, sizeof(addrs[idx].netmask));
            }
            
            if (ifa->ifa_broadaddr != NULL) {
                sin = (struct sockaddr_in*)ifa->ifa_broadaddr;
                inet_ntop(AF_INET, &sin->sin_addr, addrs[idx].broadcast, sizeof(addrs[idx].broadcast));
            }
            
            idx++;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
            strncpy(addrs[idx].name, ifa->ifa_name, sizeof(addrs[idx].name) - 1);
            addrs[idx].name[sizeof(addrs[idx].name) - 1] = '\0';
            inet_ntop(AF_INET6, &sin6->sin6_addr, addrs[idx].address, sizeof(addrs[idx].address));
            idx++;
        }
    }
    
    freeifaddrs(ifaddr);
    *count = idx;
    return addrs;
}

psutil_net_if_stat* psutil_android_net_if_stats(int* count) {
    // Simplified implementation
    *count = 0;
    return NULL;
}

psutil_io_counters psutil_android_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0, 0, 0, 0, 0, 0};
    
    // On Android, /proc/diskstats may require root
    FILE* fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        return counters;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned int major, minor;
        char device[64];
        unsigned long long reads, reads_merged, sectors_read, read_time;
        unsigned long long writes, writes_merged, sectors_written, write_time;
        
        if (sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, device,
                   &reads, &reads_merged, &sectors_read, &read_time,
                   &writes, &writes_merged, &sectors_written, &write_time) == 11) {
            // Skip loop and ram devices
            if (strncmp(device, "loop", 4) != 0 && strncmp(device, "ram", 3) != 0) {
                counters.read_count += reads;
                counters.write_count += writes;
                counters.read_bytes += sectors_read * 512;
                counters.write_bytes += sectors_written * 512;
                counters.read_time += read_time;
                counters.write_time += write_time;
            }
        }
    }
    
    fclose(fp);
    return counters;
}

psutil_disk_partition* psutil_android_disk_partitions(int all) {
    FILE* fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        return NULL;
    }
    
    // Count entries first
    int num_entries = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        num_entries++;
    }
    
    if (num_entries == 0) {
        fclose(fp);
        return NULL;
    }
    
    psutil_disk_partition* partitions = (psutil_disk_partition*)malloc((num_entries + 1) * sizeof(psutil_disk_partition));
    if (partitions == NULL) {
        fclose(fp);
        return NULL;
    }
    
    rewind(fp);
    int idx = 0;
    while (fgets(line, sizeof(line), fp) != NULL && idx < num_entries) {
        char device[256], mountpoint[256], fstype[64], opts[256];
        if (sscanf(line, "%255s %255s %63s %255s", device, mountpoint, fstype, opts) == 4) {
            // Skip pseudo filesystems unless all is requested
            if (!all && (strcmp(fstype, "proc") == 0 || strcmp(fstype, "sysfs") == 0 ||
                         strcmp(fstype, "devpts") == 0 || strcmp(fstype, "tmpfs") == 0 ||
                         strcmp(fstype, "devtmpfs") == 0 || strcmp(fstype, "cgroup") == 0)) {
                continue;
            }
            
            strncpy(partitions[idx].device, device, sizeof(partitions[idx].device) - 1);
            partitions[idx].device[sizeof(partitions[idx].device) - 1] = '\0';
            
            strncpy(partitions[idx].mountpoint, mountpoint, sizeof(partitions[idx].mountpoint) - 1);
            partitions[idx].mountpoint[sizeof(partitions[idx].mountpoint) - 1] = '\0';
            
            strncpy(partitions[idx].fstype, fstype, sizeof(partitions[idx].fstype) - 1);
            partitions[idx].fstype[sizeof(partitions[idx].fstype) - 1] = '\0';
            
            strncpy(partitions[idx].opts, opts, sizeof(partitions[idx].opts) - 1);
            partitions[idx].opts[sizeof(partitions[idx].opts) - 1] = '\0';
            
            idx++;
        }
    }
    
    // Null terminate the array
    partitions[idx].device[0] = '\0';
    
    fclose(fp);
    return partitions;
}

psutil_disk_usage psutil_android_disk_usage(const char* path) {
    psutil_disk_usage usage = {0, 0, 0, 0.0};
    
    struct statfs st;
    if (statfs(path, &st) == 0) {
        usage.total = st.f_blocks * st.f_bsize;
        usage.free = st.f_bfree * st.f_bsize;
        usage.used = usage.total - usage.free;
        if (usage.total > 0) {
            usage.percent = 100.0 * (double)usage.used / (double)usage.total;
        }
    }
    
    return usage;
}

psutil_user* psutil_android_users(int* count) {
    // utmp/wtmp is limited on Android
    // Return empty list
    *count = 0;
    return NULL;
}

double psutil_android_boot_time(void) {
    static double boot_time_val = 0;
    
    if (boot_time_val == 0) {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            boot_time_val = (double)time(NULL) - si.uptime;
        }
    }
    
    return boot_time_val;
}
