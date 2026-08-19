/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psutil.h"

#ifdef __unix__
#include <unistd.h>
#endif

// Platform-specific includes
#ifdef PSUTIL_WINDOWS
#include "arch/windows/psutil_windows.h"
#endif
#ifdef PSUTIL_LINUX
#include "arch/linux/psutil_linux.h"
#endif
#ifdef PSUTIL_ANDROID
#include "arch/android/psutil_android.h"
#endif
#ifdef PSUTIL_MACOS
#include "arch/macos/psutil_macos.h"
#endif
#ifdef PSUTIL_BSD
#include "arch/bsd/psutil_bsd.h"
#endif

// ====================================================================
// --- Process class implementation
// ====================================================================

Process* process_new(psutil_pid_t pid) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_new(pid);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_new(pid);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_new(pid);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_new(pid);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_new(pid);
#else
    Process* proc = (Process*)malloc(sizeof(Process));
    if (proc == NULL) {
        return NULL;
    }
    
    // If pid is 0, use current process PID
    if (pid == 0) {
#ifdef __unix__
        pid = getpid();
#elif defined(_WIN32)
        pid = GetCurrentProcessId();
#endif
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
#endif
}

void process_free(Process* proc) {
    if (proc == NULL) {
        return;
    }
#ifdef PSUTIL_WINDOWS
    psutil_windows_process_free(proc);
#elif defined(PSUTIL_LINUX)
    psutil_linux_process_free(proc);
#elif defined(PSUTIL_ANDROID)
    psutil_android_process_free(proc);
#elif defined(PSUTIL_MACOS)
    psutil_macos_process_free(proc);
#elif defined(PSUTIL_BSD)
    psutil_bsd_process_free(proc);
#else
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
#endif
}

psutil_pid_t process_get_pid(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return proc->pid;
}

psutil_pid_t process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_ppid(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_ppid(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_ppid(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_ppid(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_ppid(proc);
#else
    return proc->ppid;
#endif
}

const char* process_get_name(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_name(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_name(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_name(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_name(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_name(proc);
#else
    return proc->name;
#endif
}

const char* process_get_exe(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_exe(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_exe(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_exe(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_exe(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_exe(proc);
#else
    return proc->exe;
#endif
}

char** process_get_cmdline(Process* proc, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_cmdline(proc, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_cmdline(proc, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_cmdline(proc, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_cmdline(proc, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_cmdline(proc, count);
#else
    *count = 0;
    return NULL;
#endif
}

int process_get_status(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_status(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_status(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_status(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_status(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_status(proc);
#else
    return proc->status;
#endif
}

const char* process_get_username(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_username(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_username(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_username(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_username(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_username(proc);
#else
    return proc->username;
#endif
}

double process_get_create_time(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_create_time(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_create_time(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_create_time(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_create_time(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_create_time(proc);
#else
    return proc->create_time;
#endif
}

const char* process_get_cwd(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_cwd(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_cwd(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_cwd(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_cwd(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_cwd(proc);
#else
    return proc->cwd;
#endif
}

int process_get_nice(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_nice(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_nice(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_nice(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_nice(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_nice(proc);
#else
    return 0;
#endif
}

int process_set_nice(Process* proc, int value) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_set_nice(proc, value);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_set_nice(proc, value);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_set_nice(proc, value);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_set_nice(proc, value);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_set_nice(proc, value);
#else
    return 0;
#endif
}

psutil_uids process_get_uids(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_uids(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_uids(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_uids(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_uids(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_uids(proc);
#else
    psutil_uids uids = {0};
    return uids;
#endif
}

psutil_gids process_get_gids(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_gids(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_gids(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_gids(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_gids(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_gids(proc);
#else
    psutil_gids gids = {0};
    return gids;
#endif
}

const char* process_get_terminal(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_terminal(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_terminal(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_terminal(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_terminal(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_terminal(proc);
#else
    return NULL;
#endif
}

int process_get_num_fds(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_num_fds(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_num_fds(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_num_fds(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_num_fds(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_num_fds(proc);
#else
    return 0;
#endif
}

psutil_io_counters process_get_io_counters(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_io_counters(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_io_counters(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_io_counters(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_io_counters(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_io_counters(proc);
#else
    psutil_io_counters counters = {0};
    return counters;
#endif
}

int process_get_ionice(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_ionice(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_ionice(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_ionice(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_ionice(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_ionice(proc);
#else
    return 0;
#endif
}

int process_set_ionice(Process* proc, int ioclass, int value) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_set_ionice(proc, ioclass, value);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_set_ionice(proc, ioclass, value);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_set_ionice(proc, ioclass, value);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_set_ionice(proc, ioclass, value);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_set_ionice(proc, ioclass, value);
#else
    return 0;
#endif
}

int* process_get_cpu_affinity(Process* proc, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_cpu_affinity(proc, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_cpu_affinity(proc, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_cpu_affinity(proc, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_cpu_affinity(proc, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_cpu_affinity(proc, count);
#else
    *count = 0;
    return NULL;
#endif
}

int process_set_cpu_affinity(Process* proc, int* cpus, int count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_set_cpu_affinity(proc, cpus, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_set_cpu_affinity(proc, cpus, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_set_cpu_affinity(proc, cpus, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_set_cpu_affinity(proc, cpus, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_set_cpu_affinity(proc, cpus, count);
#else
    return 0;
#endif
}

int process_get_cpu_num(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_cpu_num(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_cpu_num(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_cpu_num(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_cpu_num(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_cpu_num(proc);
#else
    return 0;
#endif
}

char** process_get_environ(Process* proc, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_environ(proc, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_environ(proc, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_environ(proc, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_environ(proc, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_environ(proc, count);
#else
    *count = 0;
    return NULL;
#endif
}

int process_get_num_handles(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_num_handles(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_num_handles(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_num_handles(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_num_handles(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_num_handles(proc);
#else
    return 0;
#endif
}

psutil_ctx_switches process_get_num_ctx_switches(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_num_ctx_switches(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_num_ctx_switches(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_num_ctx_switches(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_num_ctx_switches(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_num_ctx_switches(proc);
#else
    psutil_ctx_switches switches = {0};
    return switches;
#endif
}

int process_get_num_threads(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_num_threads(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_num_threads(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_num_threads(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_num_threads(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_num_threads(proc);
#else
    return 0;
#endif
}

psutil_thread* process_get_threads(Process* proc, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_threads(proc, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_threads(proc, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_threads(proc, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_threads(proc, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_threads(proc, count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_cpu_times process_get_cpu_times(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_cpu_times(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_cpu_times(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_cpu_times(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_cpu_times(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_cpu_times(proc);
#else
    psutil_cpu_times times = {0};
    return times;
#endif
}

psutil_memory_info process_get_memory_info(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_memory_info(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_memory_info(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_memory_info(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_memory_info(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_memory_info(proc);
#else
    psutil_memory_info info = {0};
    return info;
#endif
}

psutil_memory_info process_get_memory_full_info(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_memory_full_info(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_memory_full_info(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_memory_full_info(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_memory_full_info(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_memory_full_info(proc);
#else
    psutil_memory_info info = {0};
    return info;
#endif
}

double process_get_memory_percent(Process* proc, const char* memtype) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_memory_percent(proc, memtype);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_memory_percent(proc, memtype);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_memory_percent(proc, memtype);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_memory_percent(proc, memtype);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_memory_percent(proc, memtype);
#else
    return 0.0;
#endif
}

psutil_memory_map* process_get_memory_maps(Process* proc, int* count, int grouped) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_memory_maps(proc, count, grouped);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_memory_maps(proc, count, grouped);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_memory_maps(proc, count, grouped);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_memory_maps(proc, count, grouped);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_memory_maps(proc, count, grouped);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_open_file* process_get_open_files(Process* proc, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_open_files(proc, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_open_files(proc, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_open_files(proc, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_open_files(proc, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_open_files(proc, count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_net_connection* process_get_net_connections(Process* proc, const char* kind, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_get_net_connections(proc, kind, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_get_net_connections(proc, kind, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_get_net_connections(proc, kind, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_get_net_connections(proc, kind, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_get_net_connections(proc, kind, count);
#else
    *count = 0;
    return NULL;
#endif
}

int process_send_signal(Process* proc, int sig) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_send_signal(proc, sig);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_send_signal(proc, sig);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_send_signal(proc, sig);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_send_signal(proc, sig);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_send_signal(proc, sig);
#else
    return 0;
#endif
}

int process_suspend(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_suspend(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_suspend(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_suspend(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_suspend(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_suspend(proc);
#else
    return 0;
#endif
}

int process_resume(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_resume(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_resume(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_resume(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_resume(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_resume(proc);
#else
    return 0;
#endif
}

int process_terminate(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_terminate(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_terminate(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_terminate(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_terminate(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_terminate(proc);
#else
    return 0;
#endif
}

int process_kill(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_kill(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_kill(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_kill(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_kill(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_kill(proc);
#else
    return 0;
#endif
}

int process_wait(Process* proc, double timeout) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_wait(proc, timeout);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_wait(proc, timeout);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_wait(proc, timeout);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_wait(proc, timeout);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_wait(proc, timeout);
#else
    return 0;
#endif
}

int process_is_running(Process* proc) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_process_is_running(proc);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_process_is_running(proc);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_process_is_running(proc);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_process_is_running(proc);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_process_is_running(proc);
#else
    return 1;
#endif
}

// ====================================================================
// --- System functions implementation
// ====================================================================

int pid_exists(psutil_pid_t pid) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_pid_exists(pid);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_pid_exists(pid);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_pid_exists(pid);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_pid_exists(pid);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_pid_exists(pid);
#else
    return 0;
#endif
}

psutil_pid_t* pids(int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_pids(count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_pids(count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_pids(count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_pids(count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_pids(count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_memory_info virtual_memory() {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_virtual_memory();
#elif defined(PSUTIL_LINUX)
    return psutil_linux_virtual_memory();
#elif defined(PSUTIL_ANDROID)
    return psutil_android_virtual_memory();
#elif defined(PSUTIL_MACOS)
    return psutil_macos_virtual_memory();
#elif defined(PSUTIL_BSD)
    return psutil_bsd_virtual_memory();
#else
    psutil_memory_info info = {0};
    return info;
#endif
}

psutil_memory_info swap_memory() {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_swap_memory();
#elif defined(PSUTIL_LINUX)
    return psutil_linux_swap_memory();
#elif defined(PSUTIL_ANDROID)
    return psutil_android_swap_memory();
#elif defined(PSUTIL_MACOS)
    return psutil_macos_swap_memory();
#elif defined(PSUTIL_BSD)
    return psutil_bsd_swap_memory();
#else
    psutil_memory_info info = {0};
    return info;
#endif
}

psutil_cpu_times cpu_times(int percpu) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_cpu_times(percpu);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_cpu_times(percpu);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_cpu_times(percpu);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_cpu_times(percpu);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_cpu_times(percpu);
#else
    psutil_cpu_times times = {0};
    return times;
#endif
}

double cpu_percent(double interval, int percpu) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_cpu_percent(interval, percpu);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_cpu_percent(interval, percpu);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_cpu_percent(interval, percpu);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_cpu_percent(interval, percpu);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_cpu_percent(interval, percpu);
#else
    return 0.0;
#endif
}

psutil_cpu_times cpu_times_percent(double interval, int percpu) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_cpu_times_percent(interval, percpu);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_cpu_times_percent(interval, percpu);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_cpu_times_percent(interval, percpu);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_cpu_times_percent(interval, percpu);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_cpu_times_percent(interval, percpu);
#else
    psutil_cpu_times times = {0};
    return times;
#endif
}

int cpu_count(int logical) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_cpu_count(logical);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_cpu_count(logical);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_cpu_count(logical);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_cpu_count(logical);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_cpu_count(logical);
#else
    return 0;
#endif
}

psutil_cpu_stats cpu_stats() {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_cpu_stats();
#elif defined(PSUTIL_LINUX)
    return psutil_linux_cpu_stats();
#elif defined(PSUTIL_ANDROID)
    return psutil_android_cpu_stats();
#elif defined(PSUTIL_MACOS)
    return psutil_macos_cpu_stats();
#elif defined(PSUTIL_BSD)
    return psutil_bsd_cpu_stats();
#else
    psutil_cpu_stats stats = {0};
    return stats;
#endif
}

psutil_io_counters net_io_counters(int pernic) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_net_io_counters(pernic);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_net_io_counters(pernic);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_net_io_counters(pernic);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_net_io_counters(pernic);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_net_io_counters(pernic);
#else
    psutil_io_counters counters = {0};
    return counters;
#endif
}

psutil_net_connection* net_connections(const char* kind, int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_net_connections(kind, count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_net_connections(kind, count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_net_connections(kind, count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_net_connections(kind, count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_net_connections(kind, count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_net_if_addr* net_if_addrs(int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_net_if_addrs(count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_net_if_addrs(count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_net_if_addrs(count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_net_if_addrs(count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_net_if_addrs(count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_net_if_stat* net_if_stats(int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_net_if_stats(count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_net_if_stats(count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_net_if_stats(count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_net_if_stats(count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_net_if_stats(count);
#else
    *count = 0;
    return NULL;
#endif
}

psutil_io_counters disk_io_counters(int perdisk) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_disk_io_counters(perdisk);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_disk_io_counters(perdisk);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_disk_io_counters(perdisk);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_disk_io_counters(perdisk);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_disk_io_counters(perdisk);
#else
    psutil_io_counters counters = {0};
    return counters;
#endif
}

psutil_disk_partition* disk_partitions(int all) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_disk_partitions(all);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_disk_partitions(all);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_disk_partitions(all);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_disk_partitions(all);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_disk_partitions(all);
#else
    return NULL;
#endif
}

psutil_disk_usage disk_usage(const char* path) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_disk_usage(path);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_disk_usage(path);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_disk_usage(path);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_disk_usage(path);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_disk_usage(path);
#else
    psutil_disk_usage usage = {0};
    return usage;
#endif
}

psutil_user* users(int* count) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_users(count);
#elif defined(PSUTIL_LINUX)
    return psutil_linux_users(count);
#elif defined(PSUTIL_ANDROID)
    return psutil_android_users(count);
#elif defined(PSUTIL_MACOS)
    return psutil_macos_users(count);
#elif defined(PSUTIL_BSD)
    return psutil_bsd_users(count);
#else
    *count = 0;
    return NULL;
#endif
}

double boot_time() {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_boot_time();
#elif defined(PSUTIL_LINUX)
    return psutil_linux_boot_time();
#elif defined(PSUTIL_ANDROID)
    return psutil_android_boot_time();
#elif defined(PSUTIL_MACOS)
    return psutil_macos_boot_time();
#elif defined(PSUTIL_BSD)
    return psutil_bsd_boot_time();
#else
    return 0.0;
#endif
}

int psutil_init(void) {
#ifdef PSUTIL_WINDOWS
    return psutil_windows_init();
#elif defined(PSUTIL_LINUX)
    return psutil_linux_init();
#elif defined(PSUTIL_ANDROID)
    return psutil_android_init();
#elif defined(PSUTIL_MACOS)
    return psutil_macos_init();
#elif defined(PSUTIL_BSD)
    return psutil_bsd_init();
#else
    return 0;
#endif
}

void psutil_cleanup(void) {
#ifdef PSUTIL_WINDOWS
    // Windows DLLs remain loaded for the process lifetime; no action needed.
    (void)0;
#else
    (void)0;
#endif
}

void psutil_free_string_array(char** arr, int count) {
    if (arr == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (arr[i] != NULL) {
            free(arr[i]);
        }
    }
    free(arr);
}
