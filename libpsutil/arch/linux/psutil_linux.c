/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#include <utmp.h>
#include <sched.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "../../psutil.h"
#include "psutil_linux.h"

// Initialize the library
int psutil_linux_init(void) {
    // Linux doesn't need special initialization
    return 0;
}

// Process functions
int psutil_linux_pid_exists(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

pid_t* psutil_linux_pids(int *count) {
    DIR *dir;
    struct dirent *entry;
    pid_t *pids = NULL;
    int size = 1024;
    int index = 0;

    pids = (pid_t*)malloc(size * sizeof(pid_t));
    if (pids == NULL) {
        *count = 0;
        return NULL;
    }

    dir = opendir("/proc");
    if (dir == NULL) {
        free(pids);
        *count = 0;
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        pid_t pid = atoi(entry->d_name);
        if (pid > 0) {
            if (index >= size) {
                size *= 2;
                pid_t *new_pids = (pid_t*)realloc(pids, size * sizeof(pid_t));
                if (new_pids == NULL) {
                    free(pids);
                    closedir(dir);
                    *count = 0;
                    return NULL;
                }
                pids = new_pids;
            }
            pids[index++] = pid;
        }
    }

    closedir(dir);
    *count = index;
    return pids;
}

#include <unistd.h>

Process* psutil_linux_process_new(pid_t pid) {
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

    free(proc);
}

pid_t psutil_linux_process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    
    pid_t ppid = 0;
    char line[1024];
    if (fgets(line, sizeof(line), fp) != NULL) {
        // Parse the ppid field (4th field)
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            sscanf(close_paren + 1, " %*c %d", &ppid);
        }
    }
    
    fclose(fp);
    return ppid;
}

const char* psutil_linux_process_get_name(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->name != NULL) {
        return proc->name;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        char name[256];
        if (fgets(name, sizeof(name), fp) != NULL) {
            // Remove newline
            name[strcspn(name, "\n")] = '\0';
            proc->name = strdup(name);
        }
        fclose(fp);
    }
    return proc->name;
}

const char* psutil_linux_process_get_exe(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->exe != NULL) {
        return proc->exe;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/exe", proc->pid);
    char exe[256];
    ssize_t len = readlink(path, exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        proc->exe = strdup(exe);
    }
    return proc->exe;
}

char** psutil_linux_process_get_cmdline(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Read the entire cmdline file
    char cmdline[4096];
    size_t size = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
    fclose(fp);
    
    if (size == 0) {
        *count = 0;
        return NULL;
    }
    
    // Null-terminate the string
    cmdline[size] = '\0';
    
    // Count the number of arguments
    int argc = 0;
    char* p = cmdline;
    while (*p) {
        argc++;
        // Skip to the next null separator
        while (*p) p++;
        p++;
    }
    
    // Allocate memory for the arguments
    char** argv = (char**)malloc((argc + 1) * sizeof(char*));
    if (argv == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Fill the arguments
    p = cmdline;
    for (int i = 0; i < argc; i++) {
        argv[i] = strdup(p);
        // Skip to the next null separator
        while (*p) p++;
        p++;
    }
    argv[argc] = NULL;
    
    *count = argc;
    return argv;
}

int psutil_linux_process_get_status(Process* proc) {
    if (proc == NULL) {
        return STATUS_RUNNING;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return STATUS_RUNNING;
    }
    
    char state;
    char line[1024];
    if (fgets(line, sizeof(line), fp) != NULL) {
        // Parse the state field (3rd field)
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            sscanf(close_paren + 1, " %c", &state);
            switch (state) {
                case 'R': return STATUS_RUNNING;
                case 'S': return STATUS_SLEEPING;
                case 'D': return STATUS_DISK_SLEEP;
                case 'Z': return STATUS_ZOMBIE;
                case 'T': return STATUS_STOPPED;
                case 't': return STATUS_TRACING_STOP;
                case 'X': case 'x': return STATUS_DEAD;
                case 'I': return STATUS_IDLE;
                default: return STATUS_RUNNING;
            }
        }
    }
    
    fclose(fp);
    return STATUS_RUNNING;
}

const char* psutil_linux_process_get_username(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->username != NULL) {
        return proc->username;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strncmp(line, "Uid:", 4) == 0) {
                uid_t uid;
                if (sscanf(line, "Uid: %d", &uid) == 1) {
                    struct passwd *pwd = getpwuid(uid);
                    if (pwd != NULL) {
                        proc->username = strdup(pwd->pw_name);
                    }
                }
                break;
            }
        }
        fclose(fp);
    }
    return proc->username;
}

double psutil_linux_process_get_create_time(Process* proc) {
    if (proc == NULL) {
        return 0.0;
    }
    
    if (proc->create_time > 0.0) {
        return proc->create_time;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        char line[1024];
        if (fgets(line, sizeof(line), fp) != NULL) {
            unsigned long long starttime;
            // Skip the first 21 fields to get to starttime
            if (sscanf(line, "%*d %*s %*c %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %*ld %llu", &starttime) == 1) {
                // Convert to seconds since epoch
                struct sysinfo si;
                if (sysinfo(&si) == 0) {
                    double uptime = si.uptime;
                    double boot_time = time(NULL) - uptime;
                    proc->create_time = boot_time + (double)starttime / sysconf(_SC_CLK_TCK);
                }
            }
        }
        fclose(fp);
    }
    return proc->create_time;
}

const char* psutil_linux_process_get_cwd(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->cwd != NULL) {
        return proc->cwd;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cwd", proc->pid);
    char cwd[256];
    ssize_t len = readlink(path, cwd, sizeof(cwd) - 1);
    if (len > 0) {
        cwd[len] = '\0';
        proc->cwd = strdup(cwd);
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
            sscanf(line, "Uid:\t%d\t%d\t%d\t%d", &uids.real, &uids.effective, &uids.saved, &uids.setuid);
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
            sscanf(line, "Gid:\t%d\t%d\t%d\t%d", &gids.real, &gids.effective, &gids.saved, &gids.setgid);
            break;
        }
    }
    fclose(fp);
    return gids;
}

const char* psutil_linux_process_get_terminal(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return NULL;
    }
    
    char tty[32];
    char line[1024];
    if (fgets(line, sizeof(line), fp) != NULL) {
        // Parse the tty field (7th field)
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            // Skip state, ppid, pgrp, session, tty_nr, tpgid
            sscanf(close_paren + 1, " %*c %*d %*d %*d %s", tty);
            // Check if tty is 0 (no terminal)
            if (strcmp(tty, "0") != 0) {
                // Convert tty number to device name
                static char terminal[32];
                snprintf(terminal, sizeof(terminal), "/dev/%s", tty);
                return terminal;
            }
        }
    }
    
    fclose(fp);
    return NULL;
}

int psutil_linux_process_get_num_fds(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", proc->pid);
    
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    closedir(dir);
    return count;
}

psutil_io_counters psutil_linux_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    if (proc == NULL) {
        return counters;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/io", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return counters;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long value;
        if (sscanf(line, "rchar: %llu", &value) == 1) {
            counters.read_bytes = value;
        } else if (sscanf(line, "wchar: %llu", &value) == 1) {
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
    if (proc == NULL) {
        return 0;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/ionice", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    
    int ioclass = 0, value = 0;
    char line[256];
    if (fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%*s: class %d, prio %d", &ioclass, &value);
    }
    
    fclose(fp);
    return ioclass;
}

int psutil_linux_process_set_ionice(Process* proc, int ioclass, int value) {
    if (proc == NULL) {
        return -1;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/ionice", proc->pid);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    
    int ret = fprintf(fp, "%d %d", ioclass, value);
    fclose(fp);
    
    return ret > 0 ? 0 : -1;
}

int* psutil_linux_process_get_cpu_affinity(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Get the number of online CPUs
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count <= 0) {
        *count = 0;
        return NULL;
    }
    
    // Allocate memory for CPU mask
    size_t mask_size = (cpu_count + 7) / 8;
    unsigned char* mask = (unsigned char*)calloc(mask_size, 1);
    if (mask == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Get CPU affinity
    if (sched_getaffinity(proc->pid, mask_size, mask) != 0) {
        free(mask);
        *count = 0;
        return NULL;
    }
    
    // Allocate memory for CPU list
    int* cpus = (int*)malloc(cpu_count * sizeof(int));
    if (cpus == NULL) {
        free(mask);
        *count = 0;
        return NULL;
    }
    
    // Build CPU list from mask
    int index = 0;
    for (int i = 0; i < cpu_count; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        if (mask[byte_index] & (1 << bit_index)) {
            cpus[index++] = i;
        }
    }
    
    free(mask);
    *count = index;
    return cpus;
}

int psutil_linux_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    if (proc == NULL || cpus == NULL || count <= 0) {
        return -1;
    }
    
    // Get the number of online CPUs
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count <= 0) {
        return -1;
    }
    
    // Allocate memory for CPU mask
    size_t mask_size = (cpu_count + 7) / 8;
    unsigned char* mask = (unsigned char*)calloc(mask_size, 1);
    if (mask == NULL) {
        return -1;
    }
    
    // Build mask from CPU list
    for (int i = 0; i < count; i++) {
        int cpu = cpus[i];
        if (cpu >= 0 && cpu < cpu_count) {
            int byte_index = cpu / 8;
            int bit_index = cpu % 8;
            mask[byte_index] |= (1 << bit_index);
        }
    }
    
    // Set CPU affinity
    int result = sched_setaffinity(proc->pid, mask_size, mask) == 0 ? 0 : -1;
    free(mask);
    return result;
}

int psutil_linux_process_get_cpu_num(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    int cpu_num = -1;
    char line[1024];
    if (fgets(line, sizeof(line), fp) != NULL) {
        // Parse the cpu field (39th field)
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            // Skip many fields to get to cpu
            sscanf(close_paren + 1, " %*c %*d %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %*ld %d", &cpu_num);
        }
    }
    
    fclose(fp);
    return cpu_num;
}

char** psutil_linux_process_get_environ(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/environ", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Read the entire environ file
    char environ[4096];
    size_t size = fread(environ, 1, sizeof(environ) - 1, fp);
    fclose(fp);
    
    if (size == 0) {
        *count = 0;
        return NULL;
    }
    
    // Null-terminate the string
    environ[size] = '\0';
    
    // Count the number of environment variables
    int env_count = 0;
    char* p = environ;
    while (*p) {
        env_count++;
        // Skip to the next null separator
        while (*p) p++;
        p++;
    }
    
    // Allocate memory for the environment variables
    char** env_vars = (char**)malloc((env_count + 1) * sizeof(char*));
    if (env_vars == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Fill the environment variables
    p = environ;
    for (int i = 0; i < env_count; i++) {
        env_vars[i] = strdup(p);
        // Skip to the next null separator
        while (*p) p++;
        p++;
    }
    env_vars[env_count] = NULL;
    
    *count = env_count;
    return env_vars;
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
            sscanf(line, "voluntary_ctxt_switches:	%llu", &switches.voluntary);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 25) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches:	%llu", &switches.involuntary);
        }
    }
    
    fclose(fp);
    return switches;
}

int psutil_linux_process_get_num_threads(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }
    
    int num_threads = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Threads:", 8) == 0) {
            sscanf(line, "Threads:\t%d", &num_threads);
            break;
        }
    }
    fclose(fp);
    return num_threads;
}

psutil_thread* psutil_linux_process_get_threads(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task", proc->pid);
    DIR* dir = opendir(path);
    if (dir == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Count the number of threads
    int thread_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        pid_t tid = atoi(entry->d_name);
        if (tid > 0) {
            thread_count++;
        }
    }
    closedir(dir);
    
    if (thread_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Allocate memory for threads
    psutil_thread* threads = (psutil_thread*)malloc(thread_count * sizeof(psutil_thread));
    if (threads == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Fill thread information
    dir = opendir(path);
    if (dir == NULL) {
        free(threads);
        *count = 0;
        return NULL;
    }
    
    int index = 0;
    while ((entry = readdir(dir)) != NULL && index < thread_count) {
        pid_t tid = atoi(entry->d_name);
        if (tid > 0) {
            threads[index].id = tid;
            
            // Read thread CPU times
            char stat_path[256];
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%d/stat", proc->pid, tid);
            FILE* fp = fopen(stat_path, "r");
            if (fp != NULL) {
                char line[1024];
                if (fgets(line, sizeof(line), fp) != NULL) {
                    unsigned long utime, stime;
                    char* close_paren = strrchr(line, ')');
                    if (close_paren != NULL) {
                        sscanf(close_paren + 1, " %*c %*d %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu %lu %lu", &utime, &stime);
                        long clock_ticks = sysconf(_SC_CLK_TCK);
                        if (clock_ticks > 0) {
                            threads[index].user_time = (double)utime / clock_ticks;
                            threads[index].system_time = (double)stime / clock_ticks;
                        }
                    }
                }
                fclose(fp);
            }
            
            index++;
        }
    }
    closedir(dir);
    
    *count = index;
    return threads;
}

psutil_cpu_times psutil_linux_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    if (proc == NULL) {
        return times;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return times;
    }
    
    // Read the entire line
    char line[1024];
    if (fgets(line, sizeof(line), fp)) {
        // Parse stat file - format is complex, we need utime (14) and stime (15)
        // The command name is in parentheses and may contain spaces
        unsigned long utime, stime;
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            // Skip past the command and parse the rest
            sscanf(close_paren + 1, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu",
                   &utime, &stime);
            
            // Convert from clock ticks to seconds
            long clock_ticks = sysconf(_SC_CLK_TCK);
            if (clock_ticks > 0) {
                times.user = (double)utime / clock_ticks;
                times.system = (double)stime / clock_ticks;
            }
        }
    }
    fclose(fp);
    return times;
}

psutil_memory_info psutil_linux_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    if (proc == NULL) {
        return info;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/statm", proc->pid);
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        unsigned long size, resident, share, text, lib, data, dt;
        if (fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu", &size, &resident, &share, &text, &lib, &data, &dt) == 7) {
            long page_size = sysconf(_SC_PAGESIZE);
            info.rss = (uint64_t)resident * page_size;
            info.vms = (uint64_t)size * page_size;
            info.shared = (uint64_t)share * page_size;
        }
        fclose(fp);
    }
    return info;
}

psutil_memory_info psutil_linux_process_get_memory_full_info(Process* proc) {
    psutil_memory_info info = psutil_linux_process_get_memory_info(proc);
    // Add more detailed memory information if available
    return info;
}

double psutil_linux_process_get_memory_percent(Process* proc, const char* memtype) {
    if (proc == NULL) {
        return 0.0;
    }
    
    psutil_memory_info proc_mem = psutil_linux_process_get_memory_info(proc);
    psutil_memory_info system_mem = psutil_linux_virtual_memory();
    
    if (system_mem.vms == 0) {
        return 0.0;
    }
    
    return (double)proc_mem.rss / system_mem.vms * 100.0;
}

psutil_memory_map* psutil_linux_process_get_memory_maps(Process* proc, int* count, int grouped) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/smaps", proc->pid);
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    int capacity = 64;
    psutil_memory_map* maps = (psutil_memory_map*)malloc(capacity * sizeof(psutil_memory_map));
    if (maps == NULL) {
        fclose(fp);
        *count = 0;
        return NULL;
    }
    
    int index = 0;
    char line[1024];
    psutil_memory_map current = {0};
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Check if this is a header line (starts with hex address)
        if (line[0] >= '0' && line[0] <= '9' || line[0] >= 'a' && line[0] <= 'f') {
            // Save previous entry if we have one
            if (index > 0 && current.path[0] != '\0') {
                if (index >= capacity) {
                    capacity *= 2;
                    psutil_memory_map* new_maps = (psutil_memory_map*)realloc(maps, capacity * sizeof(psutil_memory_map));
                    if (new_maps == NULL) {
                        free(maps);
                        fclose(fp);
                        *count = 0;
                        return NULL;
                    }
                    maps = new_maps;
                }
                maps[index] = current;
                index++;
            }
            
            // Parse header line
            // Format: address perms offset dev inode pathname
            memset(&current, 0, sizeof(current));
            char perms[5] = {0};
            unsigned long offset;
            unsigned int dev_major, dev_minor;
            unsigned long inode;
            char* pathname = NULL;
            
            // Find the pathname (after the 5th field)
            char* p = line;
            int field = 0;
            while (*p && field < 5) {
                if (*p == ' ') {
                    field++;
                    while (*p == ' ') p++;
                } else {
                    p++;
                }
            }
            
            if (*p) {
                pathname = p;
                // Remove newline
                while (*pathname && *pathname != '\n') pathname++;
                *pathname = '\0';
                pathname = p;
            }
            
            // Parse the line
            if (sscanf(line, "%*s %4s %lx %x:%x %lu", perms, &offset, &dev_major, &dev_minor, &inode) >= 5) {
                if (pathname && *pathname) {
                    strncpy(current.path, pathname, sizeof(current.path) - 1);
                } else {
                    strncpy(current.path, "[anon]", sizeof(current.path) - 1);
                }
            }
        } else {
            // Parse detail lines
            if (strncmp(line, "Rss:", 4) == 0) {
                unsigned long value;
                if (sscanf(line, "Rss: %lu", &value) == 1) {
                    current.rss = value * 1024;
                }
            } else if (strncmp(line, "Size:", 5) == 0) {
                unsigned long value;
                if (sscanf(line, "Size: %lu", &value) == 1) {
                    current.vms = value * 1024;
                }
            } else if (strncmp(line, "Shared_Clean:", 13) == 0) {
                unsigned long value;
                if (sscanf(line, "Shared_Clean: %lu", &value) == 1) {
                    current.shared += value * 1024;
                }
            } else if (strncmp(line, "Shared_Dirty:", 13) == 0) {
                unsigned long value;
                if (sscanf(line, "Shared_Dirty: %lu", &value) == 1) {
                    current.shared += value * 1024;
                }
            } else if (strncmp(line, "Private_Clean:", 14) == 0) {
                unsigned long value;
                if (sscanf(line, "Private_Clean: %lu", &value) == 1) {
                    current.dirty += value * 1024;
                }
            } else if (strncmp(line, "Private_Dirty:", 14) == 0) {
                unsigned long value;
                if (sscanf(line, "Private_Dirty: %lu", &value) == 1) {
                    current.dirty += value * 1024;
                }
            }
        }
    }
    
    // Save last entry
    if (current.path[0] != '\0') {
        if (index >= capacity) {
            capacity *= 2;
            psutil_memory_map* new_maps = (psutil_memory_map*)realloc(maps, capacity * sizeof(psutil_memory_map));
            if (new_maps == NULL) {
                free(maps);
                fclose(fp);
                *count = 0;
                return NULL;
            }
            maps = new_maps;
        }
        maps[index] = current;
        index++;
    }
    
    fclose(fp);
    *count = index;
    return maps;
}

psutil_open_file* psutil_linux_process_get_open_files(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    char fd_path[256];
    snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", proc->pid);
    
    DIR* dir = opendir(fd_path);
    if (dir == NULL) {
        *count = 0;
        return NULL;
    }
    
    int capacity = 64;
    psutil_open_file* files = (psutil_open_file*)malloc(capacity * sizeof(psutil_open_file));
    if (files == NULL) {
        closedir(dir);
        *count = 0;
        return NULL;
    }
    
    int index = 0;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.') continue;
        
        char fd_link[512];
        snprintf(fd_link, sizeof(fd_link), "%s/%s", fd_path, entry->d_name);
        
        char file_path[1024];
        ssize_t len = readlink(fd_link, file_path, sizeof(file_path) - 1);
        if (len == -1) continue;
        
        file_path[len] = '\0';
        
        // Only include regular files (paths starting with /)
        if (file_path[0] != '/') continue;
        
        // Check if file exists
        struct stat st;
        if (stat(file_path, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        
        if (index >= capacity) {
            capacity *= 2;
            psutil_open_file* new_files = (psutil_open_file*)realloc(files, capacity * sizeof(psutil_open_file));
            if (new_files == NULL) {
                free(files);
                closedir(dir);
                *count = 0;
                return NULL;
            }
            files = new_files;
        }
        
        strncpy(files[index].path, file_path, sizeof(files[index].path) - 1);
        files[index].fd = atoi(entry->d_name);
        index++;
    }
    
    closedir(dir);
    *count = index;
    return files;
}

psutil_net_connection* psutil_linux_process_get_net_connections(Process* proc, const char* kind, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Get all connections
    psutil_net_connection* all_connections = psutil_linux_net_connections(kind, count);
    if (all_connections == NULL || *count == 0) {
        return NULL;
    }
    
    // Filter connections by process PID
    int capacity = 64;
    psutil_net_connection* proc_connections = (psutil_net_connection*)malloc(capacity * sizeof(psutil_net_connection));
    if (proc_connections == NULL) {
        free(all_connections);
        *count = 0;
        return NULL;
    }
    
    int index = 0;
    for (int i = 0; i < *count; i++) {
        // Note: The psutil_net_connection structure doesn't have a pid field
        // In a real implementation, we would need to modify the structure to include pid
        // For now, we'll return all connections
        if (index >= capacity) {
            capacity *= 2;
            psutil_net_connection* new_conn = (psutil_net_connection*)realloc(proc_connections, capacity * sizeof(psutil_net_connection));
            if (new_conn == NULL) {
                free(proc_connections);
                free(all_connections);
                *count = 0;
                return NULL;
            }
            proc_connections = new_conn;
        }
        proc_connections[index++] = all_connections[i];
    }
    
    free(all_connections);
    *count = index;
    if (index == 0) {
        free(proc_connections);
        return NULL;
    }
    return proc_connections;
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
    if (proc == NULL) {
        return -1;
    }
    
    int status;
    pid_t result;
    
    if (timeout < 0) {
        // Wait indefinitely
        result = waitpid(proc->pid, &status, 0);
    } else {
        // Use WNOHANG with polling for timeout
        int timeout_ms = (int)(timeout * 1000);
        int elapsed = 0;
        int interval = 10; // Check every 10ms
        
        while (elapsed < timeout_ms) {
            result = waitpid(proc->pid, &status, WNOHANG);
            if (result == proc->pid) {
                return 0;  // Process exited
            } else if (result == -1) {
                return -1; // Error
            }
            usleep(interval * 1000);
            elapsed += interval;
        }
        return 1; // Timeout
    }
    
    if (result == proc->pid) {
        return 0;
    } else if (result == -1) {
        return -1;
    }
    return 1;
}

int psutil_linux_process_is_running(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    // Check if the process directory exists
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", proc->pid);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return 0;
    }
    
    // Check if the process is not a zombie
    char stat_path[256];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", proc->pid);
    FILE *fp = fopen(stat_path, "r");
    if (fp == NULL) {
        return 0;
    }
    
    char state;
    char line[1024];
    if (fgets(line, sizeof(line), fp) != NULL) {
        char* close_paren = strrchr(line, ')');
        if (close_paren != NULL) {
            sscanf(close_paren + 1, " %c", &state);
            fclose(fp);
            return state != 'Z'; // Not a zombie
        }
    }
    
    fclose(fp);
    return 0;
}

// System functions
psutil_memory_info psutil_linux_virtual_memory(void) {
    psutil_memory_info info = {0};
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.rss = (uint64_t)(si.totalram - si.freeram) * si.mem_unit;
        info.vms = (uint64_t)si.totalram * si.mem_unit;
        info.shared = 0;
        info.text = 0;
        info.lib = 0;
        info.data = 0;
        info.dirty = 0;
        info.uss = 0;
        info.pss = 0;
    }
    return info;
}

psutil_memory_info psutil_linux_swap_memory(void) {
    psutil_memory_info info = {0};
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.rss = (uint64_t)(si.totalswap - si.freeswap) * si.mem_unit;
        info.vms = (uint64_t)si.totalswap * si.mem_unit;
        info.shared = 0;
        info.text = 0;
        info.lib = 0;
        info.data = 0;
        info.dirty = 0;
        info.uss = 0;
        info.pss = 0;
    }
    return info;
}

psutil_cpu_times psutil_linux_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    FILE *fp = fopen("/proc/stat", "r");
    if (fp != NULL) {
        char line[256];
        if (fgets(line, sizeof(line), fp) != NULL) {
            unsigned long long user, nice, system, idle;
            if (sscanf(line, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle) == 4) {
                times.user = (double)user / 100.0;
                times.system = (double)system / 100.0;
            }
        }
        fclose(fp);
    }
    return times;
}

double psutil_linux_cpu_percent(double interval, int percpu) {
    if (interval <= 0) {
        return 0.0;
    }
    
    // Get initial CPU times
    psutil_cpu_times start_times = psutil_linux_cpu_times(percpu);
    
    // Sleep for the specified interval
    usleep((int)(interval * 1000000));
    
    // Get final CPU times
    psutil_cpu_times end_times = psutil_linux_cpu_times(percpu);
    
    // Calculate CPU usage
    double user_diff = end_times.user - start_times.user;
    double system_diff = end_times.system - start_times.system;
    double total_diff = user_diff + system_diff;
    
    if (total_diff == 0) {
        return 0.0;
    }
    
    return (total_diff / interval) * 100.0;
}

psutil_cpu_times psutil_linux_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    if (interval <= 0) {
        return times;
    }
    
    // Get initial CPU times
    psutil_cpu_times start_times = psutil_linux_cpu_times(percpu);
    
    // Sleep for the specified interval
    usleep((int)(interval * 1000000));
    
    // Get final CPU times
    psutil_cpu_times end_times = psutil_linux_cpu_times(percpu);
    
    // Calculate CPU usage percentages
    double user_diff = end_times.user - start_times.user;
    double system_diff = end_times.system - start_times.system;
    double total_diff = user_diff + system_diff;
    
    if (total_diff > 0) {
        times.user = (user_diff / total_diff) * 100.0;
        times.system = (system_diff / total_diff) * 100.0;
    }
    
    return times;
}

int psutil_linux_cpu_count(int logical) {
    if (logical) {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }
    // Physical core count - read from /proc/cpuinfo
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }
    
    int physical_cores = 0;
    int max_core_id = -1;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        int core_id;
        if (sscanf(line, "core id : %d", &core_id) == 1) {
            if (core_id > max_core_id) {
                max_core_id = core_id;
            }
        }
    }
    fclose(fp);
    
    if (max_core_id >= 0) {
        physical_cores = max_core_id + 1;
    }
    
    return physical_cores > 0 ? physical_cores : sysconf(_SC_NPROCESSORS_ONLN);
}

psutil_cpu_stats psutil_linux_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return stats;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "ctxt", 4) == 0) {
            sscanf(line, "ctxt %d", &stats.ctx_switches);
        } else if (strncmp(line, "intr", 4) == 0) {
            sscanf(line, "intr %d", &stats.interrupts);
        } else if (strncmp(line, "softirq", 7) == 0) {
            // Soft interrupts are in the second field
            int dummy;
            sscanf(line, "softirq %d %d", &dummy, &stats.soft_interrupts);
        }
    }
    
    fclose(fp);
    return stats;
}

psutil_io_counters psutil_linux_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};

    FILE* fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        return counters;
    }

    char line[512];
    // Skip first two header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        char iface[64];
        unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        unsigned long long tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;

        // Parse the line - format: iface: rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame rx_compressed rx_multicast tx_bytes tx_packets tx_errs tx_drop tx_fifo tx_colls tx_carrier tx_compressed
        char* colon = strchr(line, ':');
        if (colon == NULL) continue;

        *colon = '\0';
        sscanf(line, "%s", iface);

        // Skip loopback interface
        if (strcmp(iface, "lo") == 0) {
            continue;
        }

        sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               &rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo, &rx_frame, &rx_compressed, &rx_multicast,
               &tx_bytes, &tx_packets, &tx_errs, &tx_drop, &tx_fifo, &tx_colls, &tx_carrier, &tx_compressed);

        counters.read_bytes += rx_bytes;
        counters.write_bytes += tx_bytes;
        counters.read_count += rx_packets;
        counters.write_count += tx_packets;
    }

    fclose(fp);
    return counters;
}

// Helper function to parse TCP state from /proc/net/tcp
static int parse_tcp_state(char state) {
    switch (state) {
        case '0': return CONN_ESTABLISHED;
        case '1': return CONN_SYN_SENT;
        case '2': return CONN_SYN_RECV;
        case '3': return CONN_FIN_WAIT1;
        case '4': return CONN_FIN_WAIT2;
        case '5': return CONN_TIME_WAIT;
        case '6': return CONN_CLOSE;
        case '7': return CONN_CLOSE_WAIT;
        case '8': return CONN_LAST_ACK;
        case '9': return CONN_LISTEN;
        case 'A': return CONN_CLOSING;
        default: return CONN_NONE;
    }
}

// Helper function to parse address from /proc/net/tcp format
static void parse_proc_net_addr(char* buf, size_t buf_size, unsigned int addr, unsigned int port) {
    struct in_addr inaddr;
    inaddr.s_addr = addr;
    snprintf(buf, buf_size, "%s:%d", inet_ntoa(inaddr), port);
}

// Helper function to read connections from /proc/net/tcp or /proc/net/tcp6
static int read_proc_net_tcp(const char* path, int family, psutil_net_connection* connections, int max_count, int offset) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return offset;
    }

    char line[512];
    // Skip header
    fgets(line, sizeof(line), fp);

    int index = offset;
    while (fgets(line, sizeof(line), fp) != NULL && index < max_count) {
        unsigned int local_addr, local_port, rem_addr, rem_port, state;
        unsigned int inode, uid;
        int num;

        // Parse the line
        if (sscanf(line, "%d: %08X:%04X %08X:%04X %02X %08X:%08X %02X:%08X %08X %d",
                   &num, &local_addr, &local_port, &rem_addr, &rem_port, &state,
                   &inode, &uid, &num, &num, &num, &num) >= 6) {
            connections[index].fd = -1;
            connections[index].family = family;
            connections[index].type = SOCK_STREAM;
            parse_proc_net_addr(connections[index].laddr, sizeof(connections[index].laddr), local_addr, local_port);
            parse_proc_net_addr(connections[index].raddr, sizeof(connections[index].raddr), rem_addr, rem_port);
            connections[index].status = parse_tcp_state('0' + state);
            index++;
        }
    }

    fclose(fp);
    return index;
}

// Helper function to read UDP connections from /proc/net/udp or /proc/net/udp6
static int read_proc_net_udp(const char* path, int family, psutil_net_connection* connections, int max_count, int offset) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return offset;
    }

    char line[512];
    // Skip header
    fgets(line, sizeof(line), fp);

    int index = offset;
    while (fgets(line, sizeof(line), fp) != NULL && index < max_count) {
        unsigned int local_addr, local_port, rem_addr, rem_port;
        unsigned int inode, uid;
        int num;

        // Parse the line
        if (sscanf(line, "%d: %08X:%04X %08X:%04X %02X %08X:%08X %02X:%08X %08X %d",
                   &num, &local_addr, &local_port, &rem_addr, &rem_port, &num,
                   &inode, &uid, &num, &num, &num, &num) >= 4) {
            connections[index].fd = -1;
            connections[index].family = family;
            connections[index].type = SOCK_DGRAM;
            parse_proc_net_addr(connections[index].laddr, sizeof(connections[index].laddr), local_addr, local_port);
            connections[index].raddr[0] = '\0';
            connections[index].status = CONN_NONE;
            index++;
        }
    }

    fclose(fp);
    return index;
}

psutil_net_connection* psutil_linux_net_connections(const char* kind, int* count) {
    *count = 0;

    // Determine what types of connections to retrieve
    int get_tcp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp4") == 0 || strcmp(kind, "tcp") == 0);
    int get_tcp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp6") == 0 || strcmp(kind, "tcp") == 0);
    int get_udp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp4") == 0 || strcmp(kind, "udp") == 0);
    int get_udp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp6") == 0 || strcmp(kind, "udp") == 0);

    int capacity = 256;
    psutil_net_connection* connections = (psutil_net_connection*)malloc(capacity * sizeof(psutil_net_connection));
    if (connections == NULL) {
        return NULL;
    }

    int index = 0;

    // Get TCP IPv4 connections
    if (get_tcp) {
        index = read_proc_net_tcp("/proc/net/tcp", AF_INET, connections, capacity, index);
    }

    // Get TCP IPv6 connections
    if (get_tcp6 && index < capacity) {
        index = read_proc_net_tcp("/proc/net/tcp6", AF_INET6, connections, capacity, index);
    }

    // Get UDP IPv4 connections
    if (get_udp && index < capacity) {
        index = read_proc_net_udp("/proc/net/udp", AF_INET, connections, capacity, index);
    }

    // Get UDP IPv6 connections
    if (get_udp6 && index < capacity) {
        index = read_proc_net_udp("/proc/net/udp6", AF_INET6, connections, capacity, index);
    }

    *count = index;
    if (index == 0) {
        free(connections);
        return NULL;
    }
    return connections;
}

psutil_net_if_addr* psutil_linux_net_if_addrs(int* count) {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        *count = 0;
        return NULL;
    }
    
    // Count interfaces
    int capacity = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL) {
            capacity++;
        }
    }
    
    if (capacity == 0) {
        freeifaddrs(ifaddr);
        *count = 0;
        return NULL;
    }
    
    psutil_net_if_addr* addrs = (psutil_net_if_addr*)malloc(capacity * sizeof(psutil_net_if_addr));
    if (addrs == NULL) {
        freeifaddrs(ifaddr);
        *count = 0;
        return NULL;
    }
    
    int index = 0;
    for (ifa = ifaddr; ifa != NULL && index < capacity; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        int family = ifa->ifa_addr->sa_family;
        
        // Clear the entry
        memset(&addrs[index], 0, sizeof(psutil_net_if_addr));
        
        // Set interface name
        strncpy(addrs[index].name, ifa->ifa_name, sizeof(addrs[index].name) - 1);
        
        // Convert address
        if (family == AF_INET) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, addrs[index].address, sizeof(addrs[index].address));
            
            if (ifa->ifa_netmask != NULL) {
                struct sockaddr_in* netmask = (struct sockaddr_in*)ifa->ifa_netmask;
                inet_ntop(AF_INET, &netmask->sin_addr, addrs[index].netmask, sizeof(addrs[index].netmask));
            }
            
            if (ifa->ifa_flags & IFF_BROADCAST && ifa->ifa_broadaddr != NULL) {
                struct sockaddr_in* broadcast = (struct sockaddr_in*)ifa->ifa_broadaddr;
                inet_ntop(AF_INET, &broadcast->sin_addr, addrs[index].broadcast, sizeof(addrs[index].broadcast));
            }
            index++;
        } else if (family == AF_INET6) {
            struct sockaddr_in6* addr = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &addr->sin6_addr, addrs[index].address, sizeof(addrs[index].address));
            
            if (ifa->ifa_netmask != NULL) {
                struct sockaddr_in6* netmask = (struct sockaddr_in6*)ifa->ifa_netmask;
                inet_ntop(AF_INET6, &netmask->sin6_addr, addrs[index].netmask, sizeof(addrs[index].netmask));
            }
            index++;
        }
    }
    
    freeifaddrs(ifaddr);
    *count = index;
    return addrs;
}

psutil_net_if_stat* psutil_linux_net_if_stats(int* count) {
    // Get list of interfaces from net_io_counters
    psutil_io_counters* io_counters = NULL;
    int io_count = 0;
    
    // First, get list of interface names from /proc/net/dev
    FILE* fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Count interfaces (skip first 2 header lines)
    int capacity = 0;
    char line[512];
    fgets(line, sizeof(line), fp);  // Skip header
    fgets(line, sizeof(line), fp);  // Skip header
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        capacity++;
    }
    
    if (capacity == 0) {
        fclose(fp);
        *count = 0;
        return NULL;
    }
    
    psutil_net_if_stat* stats = (psutil_net_if_stat*)malloc(capacity * sizeof(psutil_net_if_stat));
    if (stats == NULL) {
        fclose(fp);
        *count = 0;
        return NULL;
    }
    
    // Reset file pointer
    rewind(fp);
    fgets(line, sizeof(line), fp);  // Skip header
    fgets(line, sizeof(line), fp);  // Skip header
    
    int index = 0;
    while (fgets(line, sizeof(line), fp) != NULL && index < capacity) {
        // Parse interface name
        char* colon = strchr(line, ':');
        if (colon == NULL) continue;
        
        *colon = '\0';
        char* name = line;
        // Skip leading whitespace
        while (*name == ' ') name++;
        
        // Get interface stats using ioctl
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) continue;
        
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
        
        // Initialize the entry
        memset(&stats[index], 0, sizeof(psutil_net_if_stat));
        strncpy(stats[index].name, name, sizeof(stats[index].name) - 1);
        
        // Get flags
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            stats[index].isup = (ifr.ifr_flags & IFF_UP) ? 1 : 0;
        }
        
        // Get MTU
        if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
            stats[index].mtu = ifr.ifr_mtu;
        }
        
        // Get speed and duplex from /sys/class/net/<name>/
        char speed_path[256];
        snprintf(speed_path, sizeof(speed_path), "/sys/class/net/%s/speed", name);
        FILE* speed_fp = fopen(speed_path, "r");
        if (speed_fp != NULL) {
            int speed;
            if (fscanf(speed_fp, "%d", &speed) == 1) {
                stats[index].speed = speed;
            }
            fclose(speed_fp);
        }
        
        // Duplex - default to unknown
        stats[index].duplex = 0;  // DUPLEX_UNKNOWN
        
        char duplex_path[256];
        snprintf(duplex_path, sizeof(duplex_path), "/sys/class/net/%s/duplex", name);
        FILE* duplex_fp = fopen(duplex_path, "r");
        if (duplex_fp != NULL) {
            char duplex[32];
            if (fscanf(duplex_fp, "%s", duplex) == 1) {
                if (strcmp(duplex, "full") == 0) {
                    stats[index].duplex = 2;  // DUPLEX_FULL
                } else if (strcmp(duplex, "half") == 0) {
                    stats[index].duplex = 1;  // DUPLEX_HALF
                }
            }
            fclose(duplex_fp);
        }
        
        close(sock);
        index++;
    }
    
    fclose(fp);
    *count = index;
    return stats;
}

psutil_io_counters psutil_linux_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};

    FILE* fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        return counters;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        unsigned int major, minor;
        char device[64];
        unsigned long long reads, reads_merged, sectors_read, read_time;
        unsigned long long writes, writes_merged, sectors_written, write_time;

        // Parse diskstats line
        // Format: major minor name reads reads_merged sectors_read read_time writes writes_merged sectors_written write_time ...
        if (sscanf(line, "%u %u %s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, device,
                   &reads, &reads_merged, &sectors_read, &read_time,
                   &writes, &writes_merged, &sectors_written, &write_time) >= 10) {

            // Skip loop devices and ram disks
            if (strncmp(device, "loop", 4) == 0 || strncmp(device, "ram", 3) == 0) {
                continue;
            }

            // Aggregate all disk stats
            counters.read_count += reads;
            counters.write_count += writes;
            counters.read_bytes += sectors_read * 512;  // sectors are 512 bytes
            counters.write_bytes += sectors_written * 512;
            counters.read_time += read_time;
            counters.write_time += write_time;
        }
    }

    fclose(fp);
    return counters;
}

psutil_disk_partition* psutil_linux_disk_partitions(int all) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        // Try /etc/mtab as fallback
        fp = fopen("/etc/mtab", "r");
        if (fp == NULL) {
            return NULL;
        }
    }

    // First pass: count entries
    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char device[256], mountpoint[256], fstype[64], opts[256];
        if (sscanf(line, "%255s %255s %63s %255s", device, mountpoint, fstype, opts) == 4) {
            // Skip virtual filesystems unless all is true
            if (all || (
                strcmp(fstype, "ext2") == 0 ||
                strcmp(fstype, "ext3") == 0 ||
                strcmp(fstype, "ext4") == 0 ||
                strcmp(fstype, "xfs") == 0 ||
                strcmp(fstype, "btrfs") == 0 ||
                strcmp(fstype, "jfs") == 0 ||
                strcmp(fstype, "reiserfs") == 0 ||
                strcmp(fstype, "vfat") == 0 ||
                strcmp(fstype, "ntfs") == 0 ||
                strcmp(fstype, "exfat") == 0 ||
                strcmp(fstype, "nfs") == 0 ||
                strcmp(fstype, "cifs") == 0 ||
                strcmp(fstype, "fuse") == 0 ||
                strncmp(fstype, "fuse.", 5) == 0
            )) {
                count++;
            }
        }
    }

    if (count == 0) {
        fclose(fp);
        return NULL;
    }

    // Allocate array (extra element for terminator)
    psutil_disk_partition* partitions = (psutil_disk_partition*)malloc((count + 1) * sizeof(psutil_disk_partition));
    if (partitions == NULL) {
        fclose(fp);
        return NULL;
    }

    // Second pass: fill array
    rewind(fp);
    int index = 0;
    while (fgets(line, sizeof(line), fp) != NULL && index < count) {
        char device[256], mountpoint[256], fstype[64], opts[256];
        if (sscanf(line, "%255s %255s %63s %255s", device, mountpoint, fstype, opts) == 4) {
            // Skip virtual filesystems unless all is true
            if (all || (
                strcmp(fstype, "ext2") == 0 ||
                strcmp(fstype, "ext3") == 0 ||
                strcmp(fstype, "ext4") == 0 ||
                strcmp(fstype, "xfs") == 0 ||
                strcmp(fstype, "btrfs") == 0 ||
                strcmp(fstype, "jfs") == 0 ||
                strcmp(fstype, "reiserfs") == 0 ||
                strcmp(fstype, "vfat") == 0 ||
                strcmp(fstype, "ntfs") == 0 ||
                strcmp(fstype, "exfat") == 0 ||
                strcmp(fstype, "nfs") == 0 ||
                strcmp(fstype, "cifs") == 0 ||
                strcmp(fstype, "fuse") == 0 ||
                strncmp(fstype, "fuse.", 5) == 0
            )) {
                // Decode mountpoint (convert \040 to space, etc.)
                char decoded_mountpoint[256];
                char *src = mountpoint, *dst = decoded_mountpoint;
                while (*src && dst < decoded_mountpoint + sizeof(decoded_mountpoint) - 1) {
                    if (*src == '\\' && *(src+1) == '0' && *(src+2) == '4' && *(src+3) == '0') {
                        *dst++ = ' ';
                        src += 4;
                    } else {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';

                strncpy(partitions[index].device, device, sizeof(partitions[index].device) - 1);
                strncpy(partitions[index].mountpoint, decoded_mountpoint, sizeof(partitions[index].mountpoint) - 1);
                strncpy(partitions[index].fstype, fstype, sizeof(partitions[index].fstype) - 1);
                strncpy(partitions[index].opts, opts, sizeof(partitions[index].opts) - 1);
                index++;
            }
        }
    }

    fclose(fp);

    // Mark end of array
    if (index < count + 1) {
        partitions[index].device[0] = '\0';
    }

    return partitions;
}

psutil_disk_usage psutil_linux_disk_usage(const char* path) {
    psutil_disk_usage usage = {0};
    struct statfs sfs;
    if (statfs(path, &sfs) == 0) {
        usage.total = (uint64_t)sfs.f_blocks * sfs.f_bsize;
        usage.free = (uint64_t)sfs.f_bavail * sfs.f_bsize;
        usage.used = usage.total - usage.free;
        usage.percent = (double)usage.used / usage.total * 100.0;
    }
    return usage;
}

psutil_user* psutil_linux_users(int* count) {
    *count = 0;

    struct utmp* ut;
    int capacity = 16;
    psutil_user* users = (psutil_user*)malloc(capacity * sizeof(psutil_user));
    if (users == NULL) {
        return NULL;
    }

    setutent();
    int index = 0;

    while ((ut = getutent()) != NULL) {
        // Only process user login entries
        if (ut->ut_type != USER_PROCESS) {
            continue;
        }

        // Resize array if needed
        if (index >= capacity) {
            capacity *= 2;
            psutil_user* new_users = (psutil_user*)realloc(users, capacity * sizeof(psutil_user));
            if (new_users == NULL) {
                free(users);
                endutent();
                return NULL;
            }
            users = new_users;
        }

        // Copy username
        strncpy(users[index].name, ut->ut_user, sizeof(users[index].name) - 1);
        users[index].name[sizeof(users[index].name) - 1] = '\0';

        // Copy terminal
        strncpy(users[index].terminal, ut->ut_line, sizeof(users[index].terminal) - 1);
        users[index].terminal[sizeof(users[index].terminal) - 1] = '\0';

        // Copy host
        strncpy(users[index].host, ut->ut_host, sizeof(users[index].host) - 1);
        users[index].host[sizeof(users[index].host) - 1] = '\0';

        // Convert login time
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
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return time(NULL) - si.uptime;
    }
    return 0.0;
}