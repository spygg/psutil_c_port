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
    // TODO: Implement
    return 0;
}

const char* psutil_bsd_process_get_name(Process* proc) {
    // TODO: Implement
    return NULL;
}

const char* psutil_bsd_process_get_exe(Process* proc) {
    // TODO: Implement
    return NULL;
}

char** psutil_bsd_process_get_cmdline(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_bsd_process_get_status(Process* proc) {
    // TODO: Implement
    return STATUS_RUNNING;
}

const char* psutil_bsd_process_get_username(Process* proc) {
    // TODO: Implement
    return NULL;
}

double psutil_bsd_process_get_create_time(Process* proc) {
    // TODO: Implement
    return 0.0;
}

const char* psutil_bsd_process_get_cwd(Process* proc) {
    // TODO: Implement
    return NULL;
}

int psutil_bsd_process_get_nice(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_set_nice(Process* proc, int value) {
    // TODO: Implement
    return 0;
}

psutil_uids psutil_bsd_process_get_uids(Process* proc) {
    psutil_uids uids = {0};
    // TODO: Implement
    return uids;
}

psutil_gids psutil_bsd_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    // TODO: Implement
    return gids;
}

const char* psutil_bsd_process_get_terminal(Process* proc) {
    // TODO: Implement
    return NULL;
}

int psutil_bsd_process_get_num_fds(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_io_counters psutil_bsd_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    // TODO: Implement
    return counters;
}

int psutil_bsd_process_get_ionice(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_set_ionice(Process* proc, int ioclass, int value) {
    // TODO: Implement
    return 0;
}

int* psutil_bsd_process_get_cpu_affinity(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_bsd_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_get_cpu_num(Process* proc) {
    // TODO: Implement
    return 0;
}

char** psutil_bsd_process_get_environ(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_bsd_process_get_num_handles(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_ctx_switches psutil_bsd_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    // TODO: Implement
    return switches;
}

int psutil_bsd_process_get_num_threads(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_thread* psutil_bsd_process_get_threads(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_cpu_times psutil_bsd_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    // TODO: Implement
    return times;
}

psutil_memory_info psutil_bsd_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    // TODO: Implement
    return info;
}

psutil_memory_info psutil_bsd_process_get_memory_full_info(Process* proc) {
    psutil_memory_info info = {0};
    // TODO: Implement
    return info;
}

double psutil_bsd_process_get_memory_percent(Process* proc, const char* memtype) {
    // TODO: Implement
    return 0.0;
}

psutil_memory_map* psutil_bsd_process_get_memory_maps(Process* proc, int* count, int grouped) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_bsd_process_get_open_files(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_connection* psutil_bsd_process_get_net_connections(Process* proc, const char* kind, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_bsd_process_send_signal(Process* proc, int sig) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_suspend(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_resume(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_terminate(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_kill(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_wait(Process* proc, double timeout) {
    // TODO: Implement
    return 0;
}

int psutil_bsd_process_is_running(Process* proc) {
    // TODO: Implement
    return 1;
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
    // TODO: Implement
    return info;
}

psutil_cpu_times psutil_bsd_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    // TODO: Implement
    return times;
}

double psutil_bsd_cpu_percent(double interval, int percpu) {
    // TODO: Implement
    return 0.0;
}

psutil_cpu_times psutil_bsd_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    // TODO: Implement
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
    // TODO: Implement
    return stats;
}

psutil_io_counters psutil_bsd_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};
    // TODO: Implement
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
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_if_stat* psutil_bsd_net_if_stats(int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_io_counters psutil_bsd_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};
    // TODO: Implement
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
    // TODO: Implement
    *count = 0;
    return NULL;
}

double psutil_bsd_boot_time(void) {
    // TODO: Implement
    return 0.0;
}