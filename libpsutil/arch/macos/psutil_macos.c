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
#include <sys/proc_info.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/syslimits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <utmpx.h>
#include "../../psutil.h"
#include "psutil_macos.h"

// Initialize the library
int psutil_macos_init(void) {
    // macOS doesn't need special initialization
    return 0;
}

// Process functions
int psutil_macos_pid_exists(pid_t pid) {
    if (pid < 0) {
        return 0;
    }
    
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        return 1;
    }
    return 0;
}

pid_t* psutil_macos_pids(int *count) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    pid_t *pids = NULL;
    struct kinfo_proc *procs = NULL;
    
    // Get the size needed
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
        *count = 0;
        return NULL;
    }
    
    procs = (struct kinfo_proc*)malloc(size);
    if (procs == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Get the process list
    if (sysctl(mib, 4, procs, &size, NULL, 0) < 0) {
        free(procs);
        *count = 0;
        return NULL;
    }
    
    int num_procs = size / sizeof(struct kinfo_proc);
    pids = (pid_t*)malloc(num_procs * sizeof(pid_t));
    if (pids == NULL) {
        free(procs);
        *count = 0;
        return NULL;
    }
    
    for (int i = 0; i < num_procs; i++) {
        pids[i] = procs[i].kp_proc.p_pid;
    }
    
    free(procs);
    *count = num_procs;
    return pids;
}

#include <unistd.h>

Process* psutil_macos_process_new(pid_t pid) {
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

void psutil_macos_process_free(Process* proc) {
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

pid_t psutil_macos_process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        return kp.kp_eproc.e_ppid;
    }
    return 0;
}

const char* psutil_macos_process_get_name(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }

    if (proc->name != NULL) {
        return proc->name;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        proc->name = strdup(kp.kp_proc.p_comm);
    }
    return proc->name;
}

const char* psutil_macos_process_get_exe(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }

    if (proc->exe != NULL) {
        return proc->exe;
    }

    char exepath[PATH_MAX];
    
    // Use proc_pidpath for macOS
    if (proc_pidpath(proc->pid, exepath, sizeof(exepath)) > 0) {
        proc->exe = strdup(exepath);
    }
    
    return proc->exe;
}

char** psutil_macos_process_get_cmdline(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Get the command line arguments using proc_pidinfo
    int mib[3] = { CTL_KERN, KERN_PROCARGS2, proc->pid };
    size_t size = 0;
    
    if (sysctl(mib, 3, NULL, &size, NULL, 0) < 0) {
        *count = 0;
        return NULL;
    }
    
    char *buf = (char*)malloc(size);
    if (buf == NULL) {
        *count = 0;
        return NULL;
    }
    
    if (sysctl(mib, 3, buf, &size, NULL, 0) < 0) {
        free(buf);
        *count = 0;
        return NULL;
    }
    
    // Parse arguments
    char **argv = NULL;
    int argc = 0;
    char *p = buf;
    
    // Skip argc
    p += sizeof(int);
    
    // Skip exec_path
    while (p < buf + size && *p != '\0') p++;
    p++;
    
    // Skip nulls
    while (p < buf + size && *p == '\0') p++;
    
    // Count arguments
    char *start = p;
    while (p < buf + size && *p != '\0') {
        if (*p == '\0') {
            argc++;
        }
        p++;
    }
    
    if (argc == 0) {
        free(buf);
        *count = 0;
        return NULL;
    }
    
    argv = (char**)malloc((argc + 1) * sizeof(char*));
    if (argv == NULL) {
        free(buf);
        *count = 0;
        return NULL;
    }
    
    // Copy arguments
    p = start;
    int i = 0;
    while (p < buf + size && i < argc) {
        argv[i] = strdup(p);
        p += strlen(p) + 1;
        i++;
    }
    argv[i] = NULL;
    
    free(buf);
    *count = argc;
    return argv;
}

int psutil_macos_process_get_status(Process* proc) {
    if (proc == NULL) {
        return STATUS_RUNNING;
    }
    
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        switch (kp.kp_proc.p_stat) {
            case SIDL:
                return STATUS_IDLE;
            case SRUN:
                return STATUS_RUNNING;
            case SSLEEP:
                return STATUS_SLEEPING;
            case SSTOP:
                return STATUS_STOPPED;
            case SZOMB:
                return STATUS_ZOMBIE;
            default:
                return STATUS_RUNNING;
        }
    }
    return STATUS_RUNNING;
}

const char* psutil_macos_process_get_username(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->username != NULL) {
        return proc->username;
    }
    
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };
    
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        uid_t uid = kp.kp_eproc.e_ucred.cr_uid;
        struct passwd *pw = getpwuid(uid);
        if (pw != NULL) {
            proc->username = strdup(pw->pw_name);
        }
    }
    
    return proc->username;
}

double psutil_macos_process_get_create_time(Process* proc) {
    if (proc == NULL) {
        return 0.0;
    }

    if (proc->create_time > 0.0) {
        return proc->create_time;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        struct timeval tv;
        tv.tv_sec = kp.kp_proc.p_starttime.tv_sec;
        tv.tv_usec = kp.kp_proc.p_starttime.tv_usec;
        proc->create_time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    }
    return proc->create_time;
}

const char* psutil_macos_process_get_cwd(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }

    if (proc->cwd != NULL) {
        return proc->cwd;
    }

    // Use proc_pidinfo with PROC_PIDVNODEPATHINFO to get cwd
    struct proc_vnodepathinfo vpi;
    int ret = proc_pidinfo(proc->pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
    
    if (ret == sizeof(vpi)) {
        proc->cwd = strdup(vpi.pvi_cdir.vip_path);
    }
    
    return proc->cwd;
}

int psutil_macos_process_get_nice(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    return getpriority(PRIO_PROCESS, proc->pid);
}

int psutil_macos_process_set_nice(Process* proc, int value) {
    if (proc == NULL) {
        return -1;
    }
    return setpriority(PRIO_PROCESS, proc->pid, value);
}

psutil_uids psutil_macos_process_get_uids(Process* proc) {
    psutil_uids uids = {0};
    if (proc == NULL) {
        return uids;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        uids.real = kp.kp_eproc.e_pcred.p_ruid;
        uids.effective = kp.kp_eproc.e_ucred.cr_uid;
        uids.saved = kp.kp_eproc.e_pcred.p_svuid;
    }
    return uids;
}

psutil_gids psutil_macos_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    if (proc == NULL) {
        return gids;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        gids.real = kp.kp_eproc.e_pcred.p_rgid;
        gids.effective = kp.kp_eproc.e_ucred.cr_gid;
        gids.saved = kp.kp_eproc.e_pcred.p_svgid;
    }
    return gids;
}

const char* psutil_macos_process_get_terminal(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }

    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, proc->pid };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) == 0 && len > 0) {
        if (kp.kp_eproc.e_tdev != NODEV) {
            static char ttyname[32];
            snprintf(ttyname, sizeof(ttyname), "/dev/tty%02x", minor(kp.kp_eproc.e_tdev));
            return ttyname;
        }
    }
    return NULL;
}

int psutil_macos_process_get_num_fds(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    int num_fds = 0;
    int mib[3];
    size_t len;

    // Get the size needed for the file descriptor list
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_NFDS;
    mib[3] = proc->pid;

    if (sysctl(mib, 4, NULL, &len, NULL, 0) == 0) {
        num_fds = len / sizeof(int);
    }

    return num_fds;
}

psutil_io_counters psutil_macos_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    // TODO: Implement
    return counters;
}

int psutil_macos_process_get_ionice(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_macos_process_set_ionice(Process* proc, int ioclass, int value) {
    // TODO: Implement
    return 0;
}

int* psutil_macos_process_get_cpu_affinity(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_macos_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    // TODO: Implement
    return 0;
}

int psutil_macos_process_get_cpu_num(Process* proc) {
    // TODO: Implement
    return 0;
}

char** psutil_macos_process_get_environ(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_macos_process_get_num_handles(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_ctx_switches psutil_macos_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    // TODO: Implement
    return switches;
}

int psutil_macos_process_get_num_threads(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    struct proc_taskinfo pti;
    int ret = proc_pidinfo(proc->pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti));

    if (ret == sizeof(pti)) {
        return pti.pti_threadnum;
    }

    return 0;
}

psutil_thread* psutil_macos_process_get_threads(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_cpu_times psutil_macos_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    if (proc == NULL) {
        return times;
    }

    struct proc_taskinfo pti;
    int ret = proc_pidinfo(proc->pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti));

    if (ret == sizeof(pti)) {
        // Convert from nanoseconds to seconds
        times.user = (double)pti.pti_total_user / 1000000000.0;
        times.system = (double)pti.pti_total_system / 1000000000.0;
    }

    return times;
}

psutil_memory_info psutil_macos_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    if (proc == NULL) {
        return info;
    }
    
    struct proc_taskinfo pti;
    int ret = proc_pidinfo(proc->pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
    
    if (ret == sizeof(pti)) {
        info.rss = pti.pti_resident_size;
        info.vms = pti.pti_virtual_size;
    }
    
    return info;
}

psutil_memory_info psutil_macos_process_get_memory_full_info(Process* proc) {
    psutil_memory_info info = {0};
    if (proc == NULL) {
        return info;
    }
    
    struct proc_taskinfo pti;
    int ret = proc_pidinfo(proc->pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
    
    if (ret == sizeof(pti)) {
        info.rss = pti.pti_resident_size;
        info.vms = pti.pti_virtual_size;
        info.shared = 0;  // Not available on macOS
        info.text = 0;    // Not available
        info.lib = 0;     // Not available
        info.data = 0;    // Not available
        info.dirty = 0;   // Not available
    }
    
    return info;
}

double psutil_macos_process_get_memory_percent(Process* proc, const char* memtype) {
    // TODO: Implement
    return 0.0;
}

psutil_memory_map* psutil_macos_process_get_memory_maps(Process* proc, int* count, int grouped) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_macos_process_get_open_files(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_connection* psutil_macos_process_get_net_connections(Process* proc, const char* kind, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_macos_process_send_signal(Process* proc, int sig) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, sig);
}

int psutil_macos_process_suspend(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGSTOP);
}

int psutil_macos_process_resume(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGCONT);
}

int psutil_macos_process_terminate(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGTERM);
}

int psutil_macos_process_kill(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    return kill(proc->pid, SIGKILL);
}

int psutil_macos_process_wait(Process* proc, double timeout) {
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

int psutil_macos_process_is_running(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    int status;
    pid_t result = waitpid(proc->pid, &status, WNOHANG);

    if (result == 0) {
        // Process is still running
        return 1;
    } else if (result == proc->pid) {
        // Process has exited
        return 0;
    } else {
        // Error or process doesn't exist
        return 0;
    }
}

// System functions
psutil_memory_info psutil_macos_virtual_memory(void) {
    psutil_memory_info info = {0};
    
    // Get total physical memory
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t total;
    size_t len = sizeof(total);
    if (sysctl(mib, 2, &total, &len, NULL, 0) == 0) {
        info.vms = total;
    }
    
    // Get vm statistics
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &count);
    
    if (kr == KERN_SUCCESS) {
        vm_size_t page_size;
        host_page_size(mach_host_self(), &page_size);
        
        info.rss = (uint64_t)vm_stats.active_count * page_size;
        info.shared = (uint64_t)vm_stats.shared_now_count * page_size;
        info.available = (uint64_t)(vm_stats.free_count + vm_stats.inactive_count) * page_size;
    }
    
    return info;
}

psutil_memory_info psutil_macos_swap_memory(void) {
    psutil_memory_info info = {0};
    
    struct xsw_usage swap_usage;
    size_t len = sizeof(swap_usage);
    int mib[2] = {CTL_VM, VM_SWAPUSAGE};
    
    if (sysctl(mib, 2, &swap_usage, &len, NULL, 0) == 0) {
        info.total = swap_usage.xsu_total;
        info.used = swap_usage.xsu_used;
        info.free = swap_usage.xsu_avail;
    }
    
    return info;
}

psutil_cpu_times psutil_macos_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    
    // Get CPU load info
    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    kern_return_t kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_info, &count);
    
    if (kr == KERN_SUCCESS) {
        // Convert from mach ticks to seconds
        // mach ticks are typically 1/100 of a second
        times.user = (double)cpu_info.cpu_ticks[CPU_STATE_USER] / 100.0;
        times.system = (double)cpu_info.cpu_ticks[CPU_STATE_SYSTEM] / 100.0;
        times.idle = (double)cpu_info.cpu_ticks[CPU_STATE_IDLE] / 100.0;
    }
    
    return times;
}

double psutil_macos_cpu_percent(double interval, int percpu) {
    // TODO: Implement
    return 0.0;
}

psutil_cpu_times psutil_macos_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    // TODO: Implement
    return times;
}

int psutil_macos_cpu_count(int logical) {
    int mib[2] = {CTL_HW, HW_NCPU};
    int count;
    size_t len = sizeof(count);
    if (sysctl(mib, 2, &count, &len, NULL, 0) == 0) {
        return count;
    }
    return 0;
}

psutil_cpu_stats psutil_macos_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    // TODO: Implement
    return stats;
}

psutil_io_counters psutil_macos_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};
    
    int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
    size_t len;
    
    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
        return counters;
    }
    
    char *buf = (char*)malloc(len);
    if (buf == NULL) {
        return counters;
    }
    
    if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
        free(buf);
        return counters;
    }
    
    char *ptr = buf;
    char *end = buf + len;
    
    while (ptr < end) {
        struct if_msghdr *ifm = (struct if_msghdr *)ptr;
        
        if (ifm->ifm_type == RTM_IFINFO2) {
            struct if_msghdr2 *ifm2 = (struct if_msghdr2 *)ifm;
            counters.bytes_sent += ifm2->ifm_data.ifi_obytes;
            counters.bytes_recv += ifm2->ifm_data.ifi_ibytes;
            counters.packets_sent += ifm2->ifm_data.ifi_opackets;
            counters.packets_recv += ifm2->ifm_data.ifi_ipackets;
            counters.errin += ifm2->ifm_data.ifi_ierrors;
            counters.errout += ifm2->ifm_data.ifi_oerrors;
            counters.dropin += ifm2->ifm_data.ifi_iqdrops;
            // Note: dropout not available in ifm_data
        }
        
        ptr += ifm->ifm_msglen;
    }
    
    free(buf);
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

psutil_net_connection* psutil_macos_net_connections(const char* kind, int* count) {
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

psutil_net_if_addr* psutil_macos_net_if_addrs(int* count) {
    *count = 0;
    
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0) {
        return NULL;
    }
    
    // Count addresses
    int num_addrs = 0;
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL) {
            num_addrs++;
        }
    }
    
    if (num_addrs == 0) {
        freeifaddrs(ifap);
        return NULL;
    }
    
    psutil_net_if_addr* addrs = (psutil_net_if_addr*)malloc(num_addrs * sizeof(psutil_net_if_addr));
    if (addrs == NULL) {
        freeifaddrs(ifap);
        return NULL;
    }
    
    int index = 0;
    for (ifa = ifap; ifa != NULL && index < num_addrs; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        strncpy(addrs[index].name, ifa->ifa_name, sizeof(addrs[index].name) - 1);
        addrs[index].name[sizeof(addrs[index].name) - 1] = '\0';
        
        addrs[index].family = ifa->ifa_addr->sa_family;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, addrs[index].address, sizeof(addrs[index].address));
            
            if (ifa->ifa_netmask != NULL) {
                sin = (struct sockaddr_in *)ifa->ifa_netmask;
                inet_ntop(AF_INET, &sin->sin_addr, addrs[index].netmask, sizeof(addrs[index].netmask));
            }
            
            if (ifa->ifa_broadaddr != NULL) {
                sin = (struct sockaddr_in *)ifa->ifa_broadaddr;
                inet_ntop(AF_INET, &sin->sin_addr, addrs[index].broadcast, sizeof(addrs[index].broadcast));
            }
            
            index++;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, addrs[index].address, sizeof(addrs[index].address));
            
            if (ifa->ifa_netmask != NULL) {
                sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
                inet_ntop(AF_INET6, &sin6->sin6_addr, addrs[index].netmask, sizeof(addrs[index].netmask));
            }
            
            index++;
        }
    }
    
    freeifaddrs(ifap);
    *count = index;
    
    if (index == 0) {
        free(addrs);
        return NULL;
    }
    
    return addrs;
}

psutil_net_if_stat* psutil_macos_net_if_stats(int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_io_counters psutil_macos_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};

    // Get disk statistics using IOKit framework
    // For now, return zeros as IOKit requires more complex implementation
    // This is a simplified version

    int mib[2] = {CTL_HW, HW_DISKSTATS};
    struct diskstats stats;
    size_t len = sizeof(stats);

    if (sysctl(mib, 2, &stats, &len, NULL, 0) == 0) {
        counters.read_count = stats.ds_rxfer;
        counters.write_count = stats.ds_wxfer;
        counters.read_bytes = stats.ds_rbytes;
        counters.write_bytes = stats.ds_wbytes;
    }

    return counters;
}

psutil_disk_partition* psutil_macos_disk_partitions(int all) {
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
            strcmp(fstype, "hfs") == 0 ||
            strcmp(fstype, "apfs") == 0 ||
            strcmp(fstype, "ufs") == 0 ||
            strcmp(fstype, "msdos") == 0 ||
            strcmp(fstype, "exfat") == 0 ||
            strcmp(fstype, "ntfs") == 0 ||
            strcmp(fstype, "nfs") == 0 ||
            strcmp(fstype, "smbfs") == 0 ||
            strcmp(fstype, "afpfs") == 0
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
            strcmp(fstype, "hfs") == 0 ||
            strcmp(fstype, "apfs") == 0 ||
            strcmp(fstype, "ufs") == 0 ||
            strcmp(fstype, "msdos") == 0 ||
            strcmp(fstype, "exfat") == 0 ||
            strcmp(fstype, "ntfs") == 0 ||
            strcmp(fstype, "nfs") == 0 ||
            strcmp(fstype, "smbfs") == 0 ||
            strcmp(fstype, "afpfs") == 0
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

psutil_disk_usage psutil_macos_disk_usage(const char* path) {
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

psutil_user* psutil_macos_users(int* count) {
    *count = 0;

    struct utmpx* ut;
    int capacity = 16;
    psutil_user* users = (psutil_user*)malloc(capacity * sizeof(psutil_user));
    if (users == NULL) {
        return NULL;
    }

    setutxent();
    int index = 0;

    while ((ut = getutxent()) != NULL) {
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
                endutxent();
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

    endutxent();
    *count = index;

    if (index == 0) {
        free(users);
        return NULL;
    }

    return users;
}

double psutil_macos_boot_time(void) {
    struct timeval boottime;
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    size_t size = sizeof(boottime);

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) == 0) {
        return (double)boottime.tv_sec + (double)boottime.tv_usec / 1000000.0;
    }

    return 0.0;
}