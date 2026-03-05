/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PSUTIL_LINUX_H
#define PSUTIL_LINUX_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <linux/sockios.h>

// Library initialization
int psutil_linux_init(void);

// Process functions
int psutil_linux_pid_exists(pid_t pid);
pid_t* psutil_linux_pids(int *count);
Process* psutil_linux_process_new(pid_t pid);
void psutil_linux_process_free(Process* proc);
pid_t psutil_linux_process_get_ppid(Process* proc);
const char* psutil_linux_process_get_name(Process* proc);
const char* psutil_linux_process_get_exe(Process* proc);
char** psutil_linux_process_get_cmdline(Process* proc, int* count);
int psutil_linux_process_get_status(Process* proc);
const char* psutil_linux_process_get_username(Process* proc);
double psutil_linux_process_get_create_time(Process* proc);
const char* psutil_linux_process_get_cwd(Process* proc);
int psutil_linux_process_get_nice(Process* proc);
int psutil_linux_process_set_nice(Process* proc, int value);
psutil_uids psutil_linux_process_get_uids(Process* proc);
psutil_gids psutil_linux_process_get_gids(Process* proc);
const char* psutil_linux_process_get_terminal(Process* proc);
int psutil_linux_process_get_num_fds(Process* proc);
psutil_io_counters psutil_linux_process_get_io_counters(Process* proc);
int psutil_linux_process_get_ionice(Process* proc);
int psutil_linux_process_set_ionice(Process* proc, int ioclass, int value);
int* psutil_linux_process_get_cpu_affinity(Process* proc, int* count);
int psutil_linux_process_set_cpu_affinity(Process* proc, int* cpus, int count);
int psutil_linux_process_get_cpu_num(Process* proc);
char** psutil_linux_process_get_environ(Process* proc, int* count);
int psutil_linux_process_get_num_handles(Process* proc);
psutil_ctx_switches psutil_linux_process_get_num_ctx_switches(Process* proc);
int psutil_linux_process_get_num_threads(Process* proc);
psutil_thread* psutil_linux_process_get_threads(Process* proc, int* count);
psutil_cpu_times psutil_linux_process_get_cpu_times(Process* proc);
psutil_memory_info psutil_linux_process_get_memory_info(Process* proc);
psutil_memory_info psutil_linux_process_get_memory_full_info(Process* proc);
double psutil_linux_process_get_memory_percent(Process* proc, const char* memtype);
psutil_memory_map* psutil_linux_process_get_memory_maps(Process* proc, int* count, int grouped);
psutil_open_file* psutil_linux_process_get_open_files(Process* proc, int* count);
psutil_net_connection* psutil_linux_process_get_net_connections(Process* proc, const char* kind, int* count);
int psutil_linux_process_send_signal(Process* proc, int sig);
int psutil_linux_process_suspend(Process* proc);
int psutil_linux_process_resume(Process* proc);
int psutil_linux_process_terminate(Process* proc);
int psutil_linux_process_kill(Process* proc);
int psutil_linux_process_wait(Process* proc, double timeout);
int psutil_linux_process_is_running(Process* proc);

// System functions
psutil_memory_info psutil_linux_virtual_memory(void);
psutil_memory_info psutil_linux_swap_memory(void);
psutil_cpu_times psutil_linux_cpu_times(int percpu);
double psutil_linux_cpu_percent(double interval, int percpu);
psutil_cpu_times psutil_linux_cpu_times_percent(double interval, int percpu);
int psutil_linux_cpu_count(int logical);
psutil_cpu_stats psutil_linux_cpu_stats(void);
psutil_io_counters psutil_linux_net_io_counters(int pernic);
psutil_net_connection* psutil_linux_net_connections(const char* kind, int* count);
psutil_net_if_addr* psutil_linux_net_if_addrs(int* count);
psutil_net_if_stat* psutil_linux_net_if_stats(int* count);
psutil_io_counters psutil_linux_disk_io_counters(int perdisk);
psutil_disk_partition* psutil_linux_disk_partitions(int all);
psutil_disk_usage psutil_linux_disk_usage(const char* path);
psutil_user* psutil_linux_users(int* count);
double psutil_linux_boot_time(void);

#endif // PSUTIL_LINUX_H