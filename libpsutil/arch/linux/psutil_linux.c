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
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <utmp.h>
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
    // TODO: Implement
    return 0;
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
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_linux_process_get_status(Process* proc) {
    // TODO: Implement
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
    // TODO: Implement
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
    // TODO: Implement
    return 0;
}

int psutil_linux_process_set_ionice(Process* proc, int ioclass, int value) {
    // TODO: Implement
    return 0;
}

int* psutil_linux_process_get_cpu_affinity(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_linux_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    // TODO: Implement
    return 0;
}

int psutil_linux_process_get_cpu_num(Process* proc) {
    // TODO: Implement
    return 0;
}

char** psutil_linux_process_get_environ(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_linux_process_get_num_handles(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_ctx_switches psutil_linux_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    // TODO: Implement
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
    // TODO: Implement
    *count = 0;
    return NULL;
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
    // TODO: Implement
    return 0.0;
}

psutil_memory_map* psutil_linux_process_get_memory_maps(Process* proc, int* count, int grouped) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_linux_process_get_open_files(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_connection* psutil_linux_process_get_net_connections(Process* proc, const char* kind, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
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
    // TODO: Implement
    return 1;
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
    // TODO: Implement
    return 0.0;
}

psutil_cpu_times psutil_linux_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    // TODO: Implement
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
    // TODO: Implement
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
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_if_stat* psutil_linux_net_if_stats(int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
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