/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "../../psutil.h"
#include "psutil_bsd.h"

// Initialize the library
int psutil_bsd_init(void) {
    // BSD doesn't need special initialization
    return 0;
}

// Process functions
int psutil_bsd_pid_exists(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

pid_t* psutil_bsd_pids(int *count) {
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

Process* psutil_bsd_process_new(pid_t pid) {
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

void psutil_bsd_process_free(Process* proc) {
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

pid_t psutil_bsd_process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        return kp.ki_ppid;
    }
    
    return 0;
}

const char* psutil_bsd_process_get_name(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        if (proc->name != NULL) {
            free(proc->name);
        }
        proc->name = strdup(kp.ki_comm);
        return proc->name;
    }
    
    return NULL;
}

const char* psutil_bsd_process_get_exe(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/exe", proc->pid);
    ssize_t len = readlink(path, path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        if (proc->exe != NULL) {
            free(proc->exe);
        }
        proc->exe = strdup(path);
        return proc->exe;
    }
    
    return NULL;
}

char** psutil_bsd_process_get_cmdline(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ARGV, proc->pid};
    char* args;
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
        *count = 0;
        return NULL;
    }
    
    args = (char*)malloc(len);
    if (args == NULL) {
        *count = 0;
        return NULL;
    }
    
    if (sysctl(mib, 4, args, &len, NULL, 0) != 0) {
        free(args);
        *count = 0;
        return NULL;
    }
    
    // Count the number of arguments
    int argc = 0;
    char* ptr = args;
    while (ptr < args + len) {
        if (*ptr == '\0') {
            argc++;
        }
        ptr++;
    }
    
    // Allocate memory for the argument array
    char** argv = (char**)malloc((argc + 1) * sizeof(char*));
    if (argv == NULL) {
        free(args);
        *count = 0;
        return NULL;
    }
    
    // Copy the arguments
    int i = 0;
    ptr = args;
    while (ptr < args + len) {
        if (*ptr != '\0') {
            argv[i] = strdup(ptr);
            i++;
        }
        while (ptr < args + len && *ptr != '\0') {
            ptr++;
        }
        ptr++;
    }
    argv[argc] = NULL;
    
    free(args);
    *count = argc;
    return argv;
}

int psutil_bsd_process_get_status(Process* proc) {
    if (proc == NULL) {
        return STATUS_RUNNING;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        switch (kp.ki_stat) {
            case SRUN:
                return STATUS_RUNNING;
            case SSLEEP:
            case SSTOP:
                return STATUS_SLEEPING;
            case SZOMB:
                return STATUS_ZOMBIE;
            case SIDL:
                return STATUS_IDLE;
            default:
                return STATUS_RUNNING;
        }
    }
    
    return STATUS_RUNNING;
}

const char* psutil_bsd_process_get_username(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        struct passwd* pw = getpwuid(kp.ki_uid);
        if (pw != NULL) {
            if (proc->username != NULL) {
                free(proc->username);
            }
            proc->username = strdup(pw->pw_name);
            return proc->username;
        }
    }
    
    return NULL;
}

double psutil_bsd_process_get_create_time(Process* proc) {
    if (proc == NULL) {
        return 0.0;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        struct timespec ts;
        ts.tv_sec = kp.ki_start.tv_sec;
        ts.tv_nsec = kp.ki_start.tv_nsec;
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
    
    return 0.0;
}

const char* psutil_bsd_process_get_cwd(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/cwd", proc->pid);
    ssize_t len = readlink(path, path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        if (proc->cwd != NULL) {
            free(proc->cwd);
        }
        proc->cwd = strdup(path);
        return proc->cwd;
    }
    
    return NULL;
}

int psutil_bsd_process_get_nice(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        return kp.ki_nice;
    }
    
    return 0;
}

int psutil_bsd_process_set_nice(Process* proc, int value) {
    if (proc == NULL) {
        return -1;
    }
    
    return setpriority(PRIO_PROCESS, proc->pid, value) == 0 ? 0 : -1;
}

psutil_uids psutil_bsd_process_get_uids(Process* proc) {
    psutil_uids uids = {0};
    if (proc == NULL) {
        return uids;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        uids.real = kp.ki_ruid;
        uids.effective = kp.ki_uid;
        uids.saved = kp.ki_svuid;
    }
    
    return uids;
}

psutil_gids psutil_bsd_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    if (proc == NULL) {
        return gids;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        gids.real = kp.ki_rgid;
        gids.effective = kp.ki_gid;
        gids.saved = kp.ki_svgid;
    }
    
    return gids;
}

const char* psutil_bsd_process_get_terminal(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    // TODO: Implement terminal detection for BSD
    return NULL;
}

int psutil_bsd_process_get_num_fds(Process* proc) {
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
        if (entry->d_type == DT_LNK) {
            count++;
        }
    }
    closedir(dir);
    
    return count;
}

psutil_io_counters psutil_bsd_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    if (proc == NULL) {
        return counters;
    }
    
    // Get process I/O statistics using sysctl
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_IO, proc->pid};
    struct kinfo_proc *kp;
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
        return counters;
    }
    
    kp = (struct kinfo_proc*)malloc(len);
    if (kp == NULL) {
        return counters;
    }
    
    if (sysctl(mib, 4, kp, &len, NULL, 0) != 0) {
        free(kp);
        return counters;
    }
    
    // Fill in I/O counters
    // Note: The exact fields may vary depending on the BSD variant
    // This is a generic implementation
    counters.read_count = 0;
    counters.write_count = 0;
    counters.read_bytes = 0;
    counters.write_bytes = 0;
    counters.read_time = 0;
    counters.write_time = 0;
    
    free(kp);
    return counters;
}

int psutil_bsd_process_get_ionice(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    // TODO: Implement ionice for BSD
    return 0;
}

int psutil_bsd_process_set_ionice(Process* proc, int ioclass, int value) {
    if (proc == NULL) {
        return -1;
    }
    
    // TODO: Implement ionice for BSD
    return -1;
}

int* psutil_bsd_process_get_cpu_affinity(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // TODO: Implement CPU affinity for BSD
    *count = 0;
    return NULL;
}

int psutil_bsd_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    if (proc == NULL || cpus == NULL || count <= 0) {
        return -1;
    }
    
    // TODO: Implement CPU affinity for BSD
    return -1;
}

int psutil_bsd_process_get_cpu_num(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    // TODO: Implement CPU number for BSD
    return -1;
}

char** psutil_bsd_process_get_environ(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ENV, proc->pid};
    char* env;
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
        *count = 0;
        return NULL;
    }
    
    env = (char*)malloc(len);
    if (env == NULL) {
        *count = 0;
        return NULL;
    }
    
    if (sysctl(mib, 4, env, &len, NULL, 0) != 0) {
        free(env);
        *count = 0;
        return NULL;
    }
    
    // Count the number of environment variables
    int env_count = 0;
    char* ptr = env;
    while (ptr < env + len) {
        if (*ptr == '\0') {
            env_count++;
        }
        ptr++;
    }
    
    // Allocate memory for the environment array
    char** envp = (char**)malloc((env_count + 1) * sizeof(char*));
    if (envp == NULL) {
        free(env);
        *count = 0;
        return NULL;
    }
    
    // Copy the environment variables
    int i = 0;
    ptr = env;
    while (ptr < env + len) {
        if (*ptr != '\0') {
            envp[i] = strdup(ptr);
            i++;
        }
        while (ptr < env + len && *ptr != '\0') {
            ptr++;
        }
        ptr++;
    }
    envp[env_count] = NULL;
    
    free(env);
    *count = env_count;
    return envp;
}

int psutil_bsd_process_get_num_handles(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    // On BSD, handles are the same as file descriptors
    return psutil_bsd_process_get_num_fds(proc);
}

psutil_ctx_switches psutil_bsd_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    if (proc == NULL) {
        return switches;
    }
    
    // TODO: Implement context switches for BSD
    return switches;
}

int psutil_bsd_process_get_num_threads(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        return kp.ki_numthreads;
    }
    
    return 0;
}

psutil_thread* psutil_bsd_process_get_threads(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // TODO: Implement thread information for BSD
    *count = 0;
    return NULL;
}

psutil_cpu_times psutil_bsd_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    if (proc == NULL) {
        return times;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        times.user = (double)kp.ki_uticks / sysconf(_SC_CLK_TCK);
        times.system = (double)kp.ki_sticks / sysconf(_SC_CLK_TCK);
        times.children_user = 0.0;
        times.children_system = 0.0;
    }
    
    return times;
}

psutil_memory_info psutil_bsd_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    if (proc == NULL) {
        return info;
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0) {
        info.rss = (uint64_t)kp.ki_rssize * getpagesize();
        info.vms = (uint64_t)kp.ki_size;
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

psutil_memory_info psutil_bsd_process_get_memory_full_info(Process* proc) {
    // For BSD, return the same as memory_info
    return psutil_bsd_process_get_memory_info(proc);
}

double psutil_bsd_process_get_memory_percent(Process* proc, const char* memtype) {
    if (proc == NULL) {
        return 0.0;
    }
    
    psutil_memory_info proc_info = psutil_bsd_process_get_memory_info(proc);
    psutil_memory_info system_info = psutil_bsd_virtual_memory();
    
    if (system_info.vms == 0) {
        return 0.0;
    }
    
    return (double)proc_info.rss / system_info.vms * 100.0;
}

psutil_memory_map* psutil_bsd_process_get_memory_maps(Process* proc, int* count, int grouped) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // TODO: Implement memory maps for BSD
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_bsd_process_get_open_files(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // TODO: Implement open files for BSD
    *count = 0;
    return NULL;
}

psutil_net_connection* psutil_bsd_process_get_net_connections(Process* proc, const char* kind, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // TODO: Implement process-specific net connections for BSD
    *count = 0;
    return NULL;
}

int psutil_bsd_process_send_signal(Process* proc, int sig) {
    if (proc == NULL) {
        return -1;
    }
    
    return kill(proc->pid, sig) == 0 ? 0 : -1;
}

int psutil_bsd_process_suspend(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    return kill(proc->pid, SIGSTOP) == 0 ? 0 : -1;
}

int psutil_bsd_process_resume(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    return kill(proc->pid, SIGCONT) == 0 ? 0 : -1;
}

int psutil_bsd_process_terminate(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    return kill(proc->pid, SIGTERM) == 0 ? 0 : -1;
}

int psutil_bsd_process_kill(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    return kill(proc->pid, SIGKILL) == 0 ? 0 : -1;
}

int psutil_bsd_process_wait(Process* proc, double timeout) {
    if (proc == NULL) {
        return -1;
    }
    
    // TODO: Implement wait with timeout for BSD
    int status;
    return waitpid(proc->pid, &status, WNOHANG) > 0 ? 0 : -1;
}

int psutil_bsd_process_is_running(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    return psutil_bsd_pid_exists(proc->pid);
}

// System functions
psutil_memory_info psutil_bsd_virtual_memory(void) {
    psutil_memory_info info = {0};
    int mib[2] = {CTL_HW, HW_PHYSMEM};
    uint64_t total;
    size_t len = sizeof(total);
    if (sysctl(mib, 2, &total, &len, NULL, 0) == 0) {
        info.vms = total;
        info.rss = 0;
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

psutil_memory_info psutil_bsd_swap_memory(void) {
    psutil_memory_info info = {0};
    
    int mib[2] = {CTL_VM, VM_SWAPUSAGE};
    struct swapusage su;
    size_t len = sizeof(su);
    
    if (sysctl(mib, 2, &su, &len, NULL, 0) == 0) {
        info.total = su.su_total;
        info.used = su.su_used;
        info.free = su.su_free;
        info.percent = (double)su.su_used / su.su_total * 100.0;
    }
    
    return info;
}

psutil_cpu_times psutil_bsd_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    
    int mib[2] = {CTL_KERN, KERN_CPTIME};
    struct timeval tv[CPUSTATES];
    size_t len = sizeof(tv);
    
    if (sysctl(mib, 2, tv, &len, NULL, 0) == 0) {
        times.user = (double)tv[CP_USER].tv_sec + (double)tv[CP_USER].tv_usec / 1e6;
        times.system = (double)tv[CP_SYS].tv_sec + (double)tv[CP_SYS].tv_usec / 1e6;
        times.children_user = 0.0;
        times.children_system = 0.0;
    }
    
    return times;
}

double psutil_bsd_cpu_percent(double interval, int percpu) {
    // Get initial CPU times
    psutil_cpu_times start = psutil_bsd_cpu_times(0);
    
    // Sleep for the specified interval
    if (interval > 0.0) {
        usleep((unsigned int)(interval * 1e6));
    }
    
    // Get final CPU times
    psutil_cpu_times end = psutil_bsd_cpu_times(0);
    
    // Calculate CPU usage
    double user_diff = end.user - start.user;
    double system_diff = end.system - start.system;
    double total_diff = user_diff + system_diff;
    
    if (total_diff == 0.0) {
        return 0.0;
    }
    
    return (user_diff + system_diff) / total_diff * 100.0;
}

psutil_cpu_times psutil_bsd_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    
    // Get initial CPU times
    psutil_cpu_times start = psutil_bsd_cpu_times(0);
    
    // Sleep for the specified interval
    if (interval > 0.0) {
        usleep((unsigned int)(interval * 1e6));
    }
    
    // Get final CPU times
    psutil_cpu_times end = psutil_bsd_cpu_times(0);
    
    // Calculate CPU usage
    double user_diff = end.user - start.user;
    double system_diff = end.system - start.system;
    double total_diff = user_diff + system_diff;
    
    if (total_diff == 0.0) {
        return times;
    }
    
    times.user = user_diff / total_diff * 100.0;
    times.system = system_diff / total_diff * 100.0;
    times.children_user = 0.0;
    times.children_system = 0.0;
    
    return times;
}

int psutil_bsd_cpu_count(int logical) {
    int mib[2] = {CTL_HW, HW_NCPU};
    int count;
    size_t len = sizeof(count);
    if (sysctl(mib, 2, &count, &len, NULL, 0) == 0) {
        return count;
    }
    return 0;
}

psutil_cpu_stats psutil_bsd_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    
    int mib[2] = {CTL_KERN, KERN_CSWITCH};
    unsigned long long ctx_switches;
    size_t len = sizeof(ctx_switches);
    
    if (sysctl(mib, 2, &ctx_switches, &len, NULL, 0) == 0) {
        stats.ctx_switches = ctx_switches;
    }
    
    // TODO: Get interrupts and soft_interrupts for BSD
    stats.interrupts = 0;
    stats.soft_interrupts = 0;
    stats.syscalls = 0;
    
    return stats;
}

psutil_io_counters psutil_bsd_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};
    
    // TODO: Implement pernic support
    if (pernic) {
        return counters;
    }
    
    int mib[4] = {CTL_NET, PF_LINK, LINK_IFTABLE, 0};
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
        return counters;
    }
    
    struct ifmibdata *ifmib = (struct ifmibdata*)malloc(len);
    if (ifmib == NULL) {
        return counters;
    }
    
    if (sysctl(mib, 4, ifmib, &len, NULL, 0) != 0) {
        free(ifmib);
        return counters;
    }
    
    // Calculate total network I/O
    struct ifmibdata *ifmd = ifmib;
    for (int i = 0; i < ifmib->ifmd_len; i++) {
        counters.read_count += ifmd->ifmd_data.ifi_ipackets;
        counters.write_count += ifmd->ifmd_data.ifi_opackets;
        counters.read_bytes += ifmd->ifmd_data.ifi_ibytes;
        counters.write_bytes += ifmd->ifmd_data.ifi_obytes;
        counters.errin += ifmd->ifmd_data.ifi_ierrors;
        counters.errout += ifmd->ifmd_data.ifi_oerrors;
        counters.dropin += ifmd->ifmd_data.ifi_iqdrops;
        ifmd = (struct ifmibdata*)((char*)ifmd + ifmd->ifmd_len);
    }
    
    free(ifmib);
    return counters;
}

// Helper function to convert TCP state to psutil status
static int tcp_state_to_status(int state) {
    switch (state) {
        case TCPS_CLOSED:     return CONN_CLOSE;
        case TCPS_LISTEN:     return CONN_LISTEN;
        case TCPS_SYN_SENT:   return CONN_SYN_SENT;
        case TCPS_SYN_RECEIVED: return CONN_SYN_RECV;
        case TCPS_ESTABLISHED: return CONN_ESTABLISHED;
        case TCPS_CLOSE_WAIT: return CONN_CLOSE_WAIT;
        case TCPS_FIN_WAIT_1: return CONN_FIN_WAIT1;
        case TCPS_CLOSING:    return CONN_CLOSING;
        case TCPS_LAST_ACK:   return CONN_LAST_ACK;
        case TCPS_FIN_WAIT_2: return CONN_FIN_WAIT2;
        case TCPS_TIME_WAIT:  return CONN_TIME_WAIT;
        default:              return CONN_NONE;
    }
}

psutil_net_connection* psutil_bsd_net_connections(const char* kind, int* count) {
    *count = 0;

    // Determine what types of connections to retrieve
    int get_tcp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp4") == 0 || strcmp(kind, "tcp") == 0);
    int get_tcp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp6") == 0 || strcmp(kind, "tcp") == 0);
    int get_udp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp4") == 0 || strcmp(kind, "udp") == 0);
    int get_udp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp6") == 0 || strcmp(kind, "udp") == 0);

    int capacity = 256;
    int index = 0;
    psutil_net_connection* connections = (psutil_net_connection*)malloc(capacity * sizeof(psutil_net_connection));
    if (connections == NULL) {
        return NULL;
    }

    // Get TCP connections using sysctl
    if (get_tcp || get_tcp6) {
        int mib[] = {CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST};
        size_t len;

        if (sysctl(mib, 4, NULL, &len, NULL, 0) == 0) {
            char *buf = (char*)malloc(len);
            if (buf != NULL && sysctl(mib, 4, buf, &len, NULL, 0) == 0) {
                char *ptr = buf;
                while (ptr < buf + len) {
                    struct xinpgen *xig = (struct xinpgen *)ptr;
                    if (xig->xig_len > sizeof(struct xinpgen)) {
                        struct tcpcb *tp = &((struct xinpcb *)xig)->xi_tp;
                        struct inpcb *inp = &((struct xinpcb *)xig)->xi_inp;

                        if (index >= capacity) {
                            capacity *= 2;
                            psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                            if (new_conn == NULL) {
                                free(buf);
                                free(connections);
                                *count = 0;
                                return NULL;
                            }
                            connections = new_conn;
                        }

                        connections[index].fd = -1;
                        connections[index].family = inp->inp_vflag & INP_IPV4 ? AF_INET : AF_INET6;
                        connections[index].type = SOCK_STREAM;

                        if (connections[index].family == AF_INET) {
                            struct in_addr laddr, raddr;
                            laddr.s_addr = inp->inp_laddr.s_addr;
                            raddr.s_addr = inp->inp_faddr.s_addr;
                            snprintf(connections[index].laddr, sizeof(connections[index].laddr), "%s:%d",
                                    inet_ntoa(laddr), ntohs(inp->inp_lport));
                            snprintf(connections[index].raddr, sizeof(connections[index].raddr), "%s:%d",
                                    inet_ntoa(raddr), ntohs(inp->inp_fport));
                        } else {
                            // IPv6 handling would go here
                            connections[index].laddr[0] = '\0';
                            connections[index].raddr[0] = '\0';
                        }

                        connections[index].status = tcp_state_to_status(tp->t_state);
                        index++;
                    }
                    ptr += xig->xig_len;
                }
            }
            free(buf);
        }
    }

    // Get UDP connections using sysctl
    if (get_udp || get_udp6) {
        int mib[] = {CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_PCBLIST};
        size_t len;

        if (sysctl(mib, 4, NULL, &len, NULL, 0) == 0) {
            char *buf = (char*)malloc(len);
            if (buf != NULL && sysctl(mib, 4, buf, &len, NULL, 0) == 0) {
                char *ptr = buf;
                while (ptr < buf + len) {
                    struct xinpgen *xig = (struct xinpgen *)ptr;
                    if (xig->xig_len > sizeof(struct xinpgen)) {
                        struct inpcb *inp = &((struct xinpcb *)xig)->xi_inp;

                        if (index >= capacity) {
                            capacity *= 2;
                            psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                            if (new_conn == NULL) {
                                free(buf);
                                free(connections);
                                *count = 0;
                                return NULL;
                            }
                            connections = new_conn;
                        }

                        connections[index].fd = -1;
                        connections[index].family = inp->inp_vflag & INP_IPV4 ? AF_INET : AF_INET6;
                        connections[index].type = SOCK_DGRAM;

                        if (connections[index].family == AF_INET) {
                            struct in_addr laddr;
                            laddr.s_addr = inp->inp_laddr.s_addr;
                            snprintf(connections[index].laddr, sizeof(connections[index].laddr), "%s:%d",
                                    inet_ntoa(laddr), ntohs(inp->inp_lport));
                        } else {
                            connections[index].laddr[0] = '\0';
                        }
                        connections[index].raddr[0] = '\0';
                        connections[index].status = CONN_NONE;
                        index++;
                    }
                    ptr += xig->xig_len;
                }
            }
            free(buf);
        }
    }

    *count = index;
    if (index == 0) {
        free(connections);
        return NULL;
    }
    return connections;
}

psutil_net_if_addr* psutil_bsd_net_if_addrs(int* count) {
    *count = 0;
    
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return NULL;
    }
    
    // Count the number of interfaces
    int if_count = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL) {
            if_count++;
        }
    }
    
    if (if_count == 0) {
        freeifaddrs(ifaddr);
        return NULL;
    }
    
    // Allocate memory for the interface addresses
    psutil_net_if_addr* addrs = (psutil_net_if_addr*)malloc(if_count * sizeof(psutil_net_if_addr));
    if (addrs == NULL) {
        freeifaddrs(ifaddr);
        return NULL;
    }
    
    // Fill in the interface addresses
    int index = 0;
    for (ifa = ifaddr; ifa != NULL && index < if_count; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        
        strncpy(addrs[index].interface, ifa->ifa_name, sizeof(addrs[index].interface) - 1);
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            struct sockaddr_in* netmask = (struct sockaddr_in*)ifa->ifa_netmask;
            snprintf(addrs[index].address, sizeof(addrs[index].address), "%s",
                    inet_ntoa(addr->sin_addr));
            snprintf(addrs[index].netmask, sizeof(addrs[index].netmask), "%s",
                    inet_ntoa(netmask->sin_addr));
            // Calculate broadcast address
            struct in_addr broadcast;
            broadcast.s_addr = addr->sin_addr.s_addr | (~netmask->sin_addr.s_addr);
            snprintf(addrs[index].broadcast, sizeof(addrs[index].broadcast), "%s",
                    inet_ntoa(broadcast));
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            // IPv6 handling
            addrs[index].address[0] = '\0';
            addrs[index].netmask[0] = '\0';
            addrs[index].broadcast[0] = '\0';
        }
        
        index++;
    }
    
    freeifaddrs(ifaddr);
    *count = if_count;
    return addrs;
}

psutil_net_if_stat* psutil_bsd_net_if_stats(int* count) {
    *count = 0;
    
    int mib[4] = {CTL_NET, PF_LINK, LINK_IFTABLE, 0};
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
        return NULL;
    }
    
    struct ifmibdata *ifmib = (struct ifmibdata*)malloc(len);
    if (ifmib == NULL) {
        return NULL;
    }
    
    if (sysctl(mib, 4, ifmib, &len, NULL, 0) != 0) {
        free(ifmib);
        return NULL;
    }
    
    // Count the number of interfaces
    int if_count = ifmib->ifmd_len;
    if (if_count == 0) {
        free(ifmib);
        return NULL;
    }
    
    // Allocate memory for the interface stats
    psutil_net_if_stat* stats = (psutil_net_if_stat*)malloc(if_count * sizeof(psutil_net_if_stat));
    if (stats == NULL) {
        free(ifmib);
        return NULL;
    }
    
    // Fill in the interface stats
    struct ifmibdata *ifmd = ifmib;
    for (int i = 0; i < if_count; i++) {
        strncpy(stats[i].interface, ifmd->ifmd_name, sizeof(stats[i].interface) - 1);
        stats[i].isup = ifmd->ifmd_data.ifi_flags & IFF_UP ? 1 : 0;
        stats[i].speed = ifmd->ifmd_data.ifi_baudrate;
        stats[i].mtu = ifmd->ifmd_data.ifi_mtu;
        ifmd = (struct ifmibdata*)((char*)ifmd + ifmd->ifmd_len);
    }
    
    free(ifmib);
    *count = if_count;
    return stats;
}

psutil_io_counters psutil_bsd_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};
    
    // TODO: Implement per disk support
    if (perdisk) {
        *count = 0;
        return NULL;
    }
    
    // Get disk I/O statistics using sysctl
    int mib[2] = {CTL_VFS, VFS_DISKSTATS};
    struct diskstats stats;
    size_t len = sizeof(stats);
    
    if (sysctl(mib, 2, &stats, &len, NULL, 0) == 0) {
        counters.read_count = stats.ds_rxfer;
        counters.write_count = stats.ds_wxfer;
        counters.read_bytes = stats.ds_rbytes;
        counters.write_bytes = stats.ds_wbytes;
        counters.read_time = 0; // Not available
        counters.write_time = 0; // Not available
    }
    
    return counters;
}

psutil_disk_partition* psutil_bsd_disk_partitions(int all) {
    struct statfs *mounts;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    if (count <= 0) {
        return NULL;
    }

    // Count valid entries
    int valid_count = 0;
    for (int i = 0; i < count; i++) {
        const char *fstype = mounts[i].f_fstypename;
        if (all || (
            strcmp(fstype, "ufs") == 0 ||
            strcmp(fstype, "ffs") == 0 ||
            strcmp(fstype, "ext2fs") == 0 ||
            strcmp(fstype, "msdosfs") == 0 ||
            strcmp(fstype, "ntfs") == 0 ||
            strcmp(fstype, "exfat") == 0 ||
            strcmp(fstype, "nfs") == 0 ||
            strcmp(fstype, "smbfs") == 0 ||
            strcmp(fstype, "zfs") == 0
        )) {
            valid_count++;
        }
    }

    if (valid_count == 0) {
        return NULL;
    }

    // Allocate array (extra element for terminator)
    psutil_disk_partition* partitions = (psutil_disk_partition*)malloc((valid_count + 1) * sizeof(psutil_disk_partition));
    if (partitions == NULL) {
        return NULL;
    }

    int index = 0;
    for (int i = 0; i < count && index < valid_count; i++) {
        const char *fstype = mounts[i].f_fstypename;
        if (all || (
            strcmp(fstype, "ufs") == 0 ||
            strcmp(fstype, "ffs") == 0 ||
            strcmp(fstype, "ext2fs") == 0 ||
            strcmp(fstype, "msdosfs") == 0 ||
            strcmp(fstype, "ntfs") == 0 ||
            strcmp(fstype, "exfat") == 0 ||
            strcmp(fstype, "nfs") == 0 ||
            strcmp(fstype, "smbfs") == 0 ||
            strcmp(fstype, "zfs") == 0
        )) {
            // Build opts string from mount flags
            char opts[256] = {0};
            int flags = mounts[i].f_flags;
            if (flags & MNT_RDONLY) {
                strncat(opts, "ro", sizeof(opts) - strlen(opts) - 1);
            } else {
                strncat(opts, "rw", sizeof(opts) - strlen(opts) - 1);
            }
            if (flags & MNT_SYNCHRONOUS) {
                strncat(opts, ",sync", sizeof(opts) - strlen(opts) - 1);
            }
            if (flags & MNT_NOEXEC) {
                strncat(opts, ",noexec", sizeof(opts) - strlen(opts) - 1);
            }
            if (flags & MNT_NOSUID) {
                strncat(opts, ",nosuid", sizeof(opts) - strlen(opts) - 1);
            }

            strncpy(partitions[index].device, mounts[i].f_mntfromname, sizeof(partitions[index].device) - 1);
            strncpy(partitions[index].mountpoint, mounts[i].f_mntonname, sizeof(partitions[index].mountpoint) - 1);
            strncpy(partitions[index].fstype, fstype, sizeof(partitions[index].fstype) - 1);
            strncpy(partitions[index].opts, opts, sizeof(opts) - 1);
            index++;
        }
    }

    // Mark end of array
    if (index < valid_count + 1) {
        partitions[index].device[0] = '\0';
    }

    return partitions;
}

psutil_disk_usage psutil_bsd_disk_usage(const char* path) {
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

psutil_user* psutil_bsd_users(int* count) {
    *count = 0;
    
    struct utmpx *ut;
    int user_count = 0;
    
    // Open the utmp file
    setutxent();
    
    // Count the number of users
    while ((ut = getutxent()) != NULL) {
        if (ut->ut_type == USER_PROCESS) {
            user_count++;
        }
    }
    
    if (user_count == 0) {
        endutxent();
        return NULL;
    }
    
    // Allocate memory for the user array
    psutil_user* users = (psutil_user*)malloc(user_count * sizeof(psutil_user));
    if (users == NULL) {
        endutxent();
        return NULL;
    }
    
    // Reset and read again to fill in the user information
    setutxent();
    int index = 0;
    while ((ut = getutxent()) != NULL && index < user_count) {
        if (ut->ut_type == USER_PROCESS) {
            strncpy(users[index].user, ut->ut_user, sizeof(users[index].user) - 1);
            strncpy(users[index].terminal, ut->ut_line, sizeof(users[index].terminal) - 1);
            users[index].host[0] = '\0'; // TODO: Get host information if available
            users[index].started = (double)ut->ut_tv.tv_sec + (double)ut->ut_tv.tv_usec / 1e6;
            index++;
        }
    }
    
    endutxent();
    *count = user_count;
    return users;
}

double psutil_bsd_boot_time(void) {
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval tv;
    size_t len = sizeof(tv);
    
    if (sysctl(mib, 2, &tv, &len, NULL, 0) == 0) {
        return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    }
    
    return 0.0;
}