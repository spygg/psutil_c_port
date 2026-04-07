/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PSUTIL_H
#define PSUTIL_H

#include <stdint.h>

// Platform detection
#if !defined(PSUTIL_WINDOWS) && !defined(PSUTIL_LINUX) && !defined(PSUTIL_MACOS) && !defined(PSUTIL_BSD) && !defined(PSUTIL_ANDROID)
    #if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
        #define PSUTIL_WINDOWS
    #elif defined(__ANDROID__) || defined(ANDROID)
        #define PSUTIL_ANDROID
    #elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
        #define PSUTIL_LINUX
    #elif defined(__APPLE__) && defined(__MACH__)
        #define PSUTIL_MACOS
    #elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
        #define PSUTIL_BSD
    #endif
#endif

// ====================================================================
// --- Constants
// ====================================================================

// Process status constants
#define STATUS_RUNNING 0
#define STATUS_IDLE 1
#define STATUS_SLEEPING 2
#define STATUS_DISK_SLEEP 3
#define STATUS_STOPPED 4
#define STATUS_TRACING_STOP 5
#define STATUS_ZOMBIE 6
#define STATUS_DEAD 7
#define STATUS_WAKING 8
#define STATUS_LOCKED 9
#define STATUS_WAITING 10
#define STATUS_PARKED 11

// Connection status constants
#define CONN_ESTABLISHED 1
#define CONN_SYN_SENT 2
#define CONN_SYN_RECV 3
#define CONN_FIN_WAIT1 4
#define CONN_FIN_WAIT2 5
#define CONN_TIME_WAIT 6
#define CONN_CLOSE 7
#define CONN_CLOSE_WAIT 8
#define CONN_LAST_ACK 9
#define CONN_LISTEN 10
#define CONN_CLOSING 11
#define CONN_NONE 128

// Network interface duplex constants
#define NIC_DUPLEX_FULL 0
#define NIC_DUPLEX_HALF 1
#define NIC_DUPLEX_UNKNOWN 2

// Power time constants
#define POWER_TIME_UNKNOWN -1
#define POWER_TIME_UNLIMITED -2

// ====================================================================
// --- Types
// ====================================================================

typedef struct {
    uint32_t real;
    uint32_t effective;
    uint32_t saved;
    uint32_t setuid;
} psutil_uids;

typedef struct {
    uint32_t real;
    uint32_t effective;
    uint32_t saved;
    uint32_t setgid;
} psutil_gids;

typedef struct {
    double user;
    double system;
    double children_user;
    double children_system;
} psutil_cpu_times;

typedef struct {
    uint64_t rss;
    uint64_t vms;
    uint64_t shared;
    uint64_t text;
    uint64_t lib;
    uint64_t data;
    uint64_t dirty;
    uint64_t uss;
    uint64_t pss;
} psutil_memory_info;

typedef struct {
    uint64_t read_count;
    uint64_t write_count;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t read_time;
    uint64_t write_time;
} psutil_io_counters;

typedef struct {
    uint64_t voluntary;
    uint64_t involuntary;
} psutil_ctx_switches;

typedef struct {
    int id;
    double user_time;
    double system_time;
} psutil_thread;

typedef struct {
    int fd;
    int family;
    int type;
    char laddr[256];
    char raddr[256];
    int status;
} psutil_net_connection;

typedef struct {
    char path[1024];
    int fd;
} psutil_open_file;

typedef struct {
    char path[1024];
    uint64_t rss;
    uint64_t vms;
    uint64_t shared;
    uint64_t text;
    uint64_t lib;
    uint64_t data;
    uint64_t dirty;
} psutil_memory_map;

// ====================================================================
// --- Process class
// ====================================================================

typedef struct Process {
    uint32_t pid;
    char* name;
    char* exe;
    char* cwd;
    char* username;
    double create_time;
    uint32_t ppid;
    int status;
} Process;

// Create a new Process object for the given PID
// If pid is 0, use the current process PID
Process* process_new(uint32_t pid);

// Free a Process object
void process_free(Process* proc);

// Get the process PID
uint32_t process_get_pid(Process* proc);

// Get the process parent PID
uint32_t process_get_ppid(Process* proc);

// Get the process name
const char* process_get_name(Process* proc);

// Get the process executable path
const char* process_get_exe(Process* proc);

// Get the process command line
char** process_get_cmdline(Process* proc, int* count);

// Get the process status
int process_get_status(Process* proc);

// Get the process username
const char* process_get_username(Process* proc);

// Get the process creation time
double process_get_create_time(Process* proc);

// Get the process current working directory
const char* process_get_cwd(Process* proc);

// Get the process niceness (priority)
int process_get_nice(Process* proc);

// Set the process niceness (priority)
int process_set_nice(Process* proc, int value);

// Get the process UIDs
psutil_uids process_get_uids(Process* proc);

// Get the process GIDs
psutil_gids process_get_gids(Process* proc);

// Get the process terminal
const char* process_get_terminal(Process* proc);

// Get the number of file descriptors opened by the process
int process_get_num_fds(Process* proc);

// Get the process I/O statistics
psutil_io_counters process_get_io_counters(Process* proc);

// Get the process I/O niceness (priority)
int process_get_ionice(Process* proc);

// Set the process I/O niceness (priority)
int process_set_ionice(Process* proc, int ioclass, int value);

// Get the process CPU affinity
int* process_get_cpu_affinity(Process* proc, int* count);

// Set the process CPU affinity
int process_set_cpu_affinity(Process* proc, int* cpus, int count);

// Get the CPU number the process is currently running on
int process_get_cpu_num(Process* proc);

// Get the process environment variables
char** process_get_environ(Process* proc, int* count);

// Get the number of handles opened by the process (Windows only)
int process_get_num_handles(Process* proc);

// Get the number of context switches performed by the process
psutil_ctx_switches process_get_num_ctx_switches(Process* proc);

// Get the number of threads used by the process
int process_get_num_threads(Process* proc);

// Get the threads opened by the process
psutil_thread* process_get_threads(Process* proc, int* count);

// Get the process CPU times
psutil_cpu_times process_get_cpu_times(Process* proc);

// Get the process memory information
psutil_memory_info process_get_memory_info(Process* proc);

// Get the process memory information with additional metrics
psutil_memory_info process_get_memory_full_info(Process* proc);

// Get the process memory utilization as a percentage
double process_get_memory_percent(Process* proc, const char* memtype);

// Get the process memory maps
psutil_memory_map* process_get_memory_maps(Process* proc, int* count, int grouped);

// Get the files opened by the process
psutil_open_file* process_get_open_files(Process* proc, int* count);

// Get the socket connections opened by the process
psutil_net_connection* process_get_net_connections(Process* proc, const char* kind, int* count);

// Send a signal to the process
int process_send_signal(Process* proc, int sig);

// Suspend the process
int process_suspend(Process* proc);

// Resume the process
int process_resume(Process* proc);

// Terminate the process
int process_terminate(Process* proc);

// Kill the process
int process_kill(Process* proc);

// Wait for the process to terminate
int process_wait(Process* proc, double timeout);

// Check if the process is running
int process_is_running(Process* proc);

// ====================================================================
// --- System functions
// ====================================================================

// Check if a PID exists
int pid_exists(uint32_t pid);

// Get a list of all running process PIDs
uint32_t* pids(int* count);

// Get virtual memory information
psutil_memory_info virtual_memory();

// Get swap memory information
psutil_memory_info swap_memory();

// Get CPU times
psutil_cpu_times cpu_times(int percpu);

// Get CPU percent utilization
double cpu_percent(double interval, int percpu);

// Get CPU times percent utilization
psutil_cpu_times cpu_times_percent(double interval, int percpu);

// Get the number of CPUs
int cpu_count(int logical);

// Get CPU statistics
typedef struct {
    int ctx_switches;
    int interrupts;
    int soft_interrupts;
    int syscalls;
} psutil_cpu_stats;

psutil_cpu_stats cpu_stats();

// Get network I/O counters
psutil_io_counters net_io_counters(int pernic);

// Get network connections
psutil_net_connection* net_connections(const char* kind, int* count);

// Get network interface addresses
typedef struct {
    char name[64];
    char address[256];
    char netmask[256];
    char broadcast[256];
} psutil_net_if_addr;

psutil_net_if_addr* net_if_addrs(int* count);

// Get network interface statistics
typedef struct {
    char name[64];
    int isup;
    int duplex;
    int speed;
    int mtu;
} psutil_net_if_stat;

psutil_net_if_stat* net_if_stats(int* count);

// Get disk I/O counters
psutil_io_counters disk_io_counters(int perdisk);

// Get disk partitions
typedef struct {
    char device[256];
    char mountpoint[256];
    char fstype[64];
    char opts[256];
} psutil_disk_partition;

psutil_disk_partition* disk_partitions(int all);

// Get disk usage
typedef struct {
    uint64_t total;
    uint64_t used;
    uint64_t free;
    double percent;
} psutil_disk_usage;

psutil_disk_usage disk_usage(const char* path);

// Get users
typedef struct {
    char name[64];
    char terminal[64];
    char host[256];
    double started;
} psutil_user;

psutil_user* users(int* count);

// Get boot time
double boot_time();

// ====================================================================
// --- Library initialization
// ====================================================================
int psutil_init(void);

// ====================================================================
// --- Platform-specific functions
// ====================================================================

#ifdef PSUTIL_WINDOWS
// Windows-specific functions
#endif

#ifdef PSUTIL_LINUX
// Linux-specific functions
#endif

#ifdef PSUTIL_ANDROID
// Android-specific functions
#endif

#ifdef PSUTIL_MACOS
// macOS-specific functions
#endif

#ifdef PSUTIL_BSD
// BSD-specific functions
#endif

#endif // PSUTIL_H
