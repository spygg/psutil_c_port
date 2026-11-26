/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PSUTIL_H
#define PSUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

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
// Prefixed names are canonical. Short names are kept for backward
// compatibility; define PSUTIL_NO_SHORT_MACROS before including this
// header to keep the global namespace clean (Windows SDK / Qt safe).
// The library itself always uses the prefixed names.

// Process status constants
#define PSUTIL_STATUS_RUNNING 0
#define PSUTIL_STATUS_IDLE 1
#define PSUTIL_STATUS_SLEEPING 2
#define PSUTIL_STATUS_DISK_SLEEP 3
#define PSUTIL_STATUS_STOPPED 4
#define PSUTIL_STATUS_TRACING_STOP 5
#define PSUTIL_STATUS_ZOMBIE 6
#define PSUTIL_STATUS_DEAD 7
#define PSUTIL_STATUS_WAKING 8
#define PSUTIL_STATUS_LOCKED 9
#define PSUTIL_STATUS_WAITING 10
#define PSUTIL_STATUS_PARKED 11
#define PSUTIL_STATUS_WAKE_KILL 12

// Connection status constants
#define PSUTIL_CONN_ESTABLISHED 1
#define PSUTIL_CONN_SYN_SENT 2
#define PSUTIL_CONN_SYN_RECV 3
#define PSUTIL_CONN_FIN_WAIT1 4
#define PSUTIL_CONN_FIN_WAIT2 5
#define PSUTIL_CONN_TIME_WAIT 6
#define PSUTIL_CONN_CLOSE 7
#define PSUTIL_CONN_CLOSE_WAIT 8
#define PSUTIL_CONN_LAST_ACK 9
#define PSUTIL_CONN_LISTEN 10
#define PSUTIL_CONN_CLOSING 11
#define PSUTIL_CONN_NONE 128

// Network interface duplex constants
#define PSUTIL_NIC_DUPLEX_FULL 0
#define PSUTIL_NIC_DUPLEX_HALF 1
#define PSUTIL_NIC_DUPLEX_UNKNOWN 2

// Power time constants
#define PSUTIL_POWER_TIME_UNKNOWN -1
#define PSUTIL_POWER_TIME_UNLIMITED -2

#ifndef PSUTIL_NO_SHORT_MACROS
#define STATUS_RUNNING PSUTIL_STATUS_RUNNING
#define STATUS_IDLE PSUTIL_STATUS_IDLE
#define STATUS_SLEEPING PSUTIL_STATUS_SLEEPING
#define STATUS_DISK_SLEEP PSUTIL_STATUS_DISK_SLEEP
#define STATUS_STOPPED PSUTIL_STATUS_STOPPED
#define STATUS_TRACING_STOP PSUTIL_STATUS_TRACING_STOP
#define STATUS_ZOMBIE PSUTIL_STATUS_ZOMBIE
#define STATUS_DEAD PSUTIL_STATUS_DEAD
#define STATUS_WAKING PSUTIL_STATUS_WAKING
#define STATUS_LOCKED PSUTIL_STATUS_LOCKED
#define STATUS_WAITING PSUTIL_STATUS_WAITING
#define STATUS_PARKED PSUTIL_STATUS_PARKED
#define STATUS_WAKE_KILL PSUTIL_STATUS_WAKE_KILL
#define CONN_ESTABLISHED PSUTIL_CONN_ESTABLISHED
#define CONN_SYN_SENT PSUTIL_CONN_SYN_SENT
#define CONN_SYN_RECV PSUTIL_CONN_SYN_RECV
#define CONN_FIN_WAIT1 PSUTIL_CONN_FIN_WAIT1
#define CONN_FIN_WAIT2 PSUTIL_CONN_FIN_WAIT2
#define CONN_TIME_WAIT PSUTIL_CONN_TIME_WAIT
#define CONN_CLOSE PSUTIL_CONN_CLOSE
#define CONN_CLOSE_WAIT PSUTIL_CONN_CLOSE_WAIT
#define CONN_LAST_ACK PSUTIL_CONN_LAST_ACK
#define CONN_LISTEN PSUTIL_CONN_LISTEN
#define CONN_CLOSING PSUTIL_CONN_CLOSING
#define CONN_NONE PSUTIL_CONN_NONE
#define NIC_DUPLEX_FULL PSUTIL_NIC_DUPLEX_FULL
#define NIC_DUPLEX_HALF PSUTIL_NIC_DUPLEX_HALF
#define NIC_DUPLEX_UNKNOWN PSUTIL_NIC_DUPLEX_UNKNOWN
#define POWER_TIME_UNKNOWN PSUTIL_POWER_TIME_UNKNOWN
#define POWER_TIME_UNLIMITED PSUTIL_POWER_TIME_UNLIMITED
#endif // PSUTIL_NO_SHORT_MACROS

// ====================================================================
// --- Types
// ====================================================================

// Cross-platform process ID type
// pid_t on Unix is signed int; DWORD on Windows is unsigned.
// Real-world PIDs fit comfortably in int32_t on all supported platforms.
typedef int32_t psutil_pid_t;

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
    /* Aggregated system-wide idle time (seconds). Needed to compute a
     * meaningful percentage; per-process cpu_times leave it at 0. */
    double idle;
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
    /* Memory the OS considers immediately reclaimable. macOS filled this in
     * and failed to compile against a struct that lacked it; Linux and
     * Windows now report it too, and it is what "used" is derived from. */
    uint64_t available;
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
    uint32_t id;
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
    psutil_pid_t pid;
    char* name;
    char* exe;
    char* cwd;
    char* username;
    double create_time;
    psutil_pid_t ppid;
    int status;
    /* Backend-private state (e.g. a cached /proc/<pid>/stat parse).
     * Owned by process_new()/process_free()/process_refresh(); the caller
     * must never dereference or free it. */
    void* _platform;
} Process;

// Create a new Process object for the given PID
// If pid is 0, use the current process PID
Process* process_new(psutil_pid_t pid);

// Free a Process object
void process_free(Process* proc);

// Drop every cached field of the object so the next getter re-reads it.
// Call it before reusing a Process across a long period: PIDs are
// recycled and cached name/exe/username/create_time would otherwise
// describe a previous owner of the same PID.
void process_refresh(Process* proc);

// Get the process PID
psutil_pid_t process_get_pid(Process* proc);

// Get the process parent PID
psutil_pid_t process_get_ppid(Process* proc);

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

// Free a string array returned by process_get_cmdline or process_get_environ
void psutil_free_string_array(char** arr, int count);

// Arrays of structs (pids, net_connections, net_if_addrs, net_if_stats,
// disk_partitions, users, memory_maps, open_files, threads, cpu affinity)
// are released with a single free() call.

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
int pid_exists(psutil_pid_t pid);

// Get a list of all running process PIDs
psutil_pid_t* pids(int* count);

// Get virtual memory information
psutil_memory_info virtual_memory(void);

// Get swap memory information
psutil_memory_info swap_memory(void);

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
    uint64_t ctx_switches;
    uint64_t interrupts;
    uint64_t soft_interrupts;
    uint64_t syscalls;
} psutil_cpu_stats;

psutil_cpu_stats cpu_stats(void);

// Get network I/O counters
psutil_io_counters net_io_counters(int pernic);

// Get network connections
psutil_net_connection* net_connections(const char* kind, int* count);

// Get network interface addresses
typedef struct {
    char name[64];
    int family;
    char address[256];
    char netmask[256];
    char broadcast[256];
    /* Hardware address ("aa:bb:cc:dd:ee:ff"). Filled for AF_PACKET rows and
     * left empty on plain AF_INET / AF_INET6 rows. */
    char mac[64];
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

// Get disk partitions. Returns an array of *count entries (never with a
// sentinel element); caller must free() it. *count is 0 when nothing is
// found or on error (see psutil_last_error()).
psutil_disk_partition* disk_partitions(int all, int* count);

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
double boot_time(void);

// ====================================================================
// --- Library initialization and error reporting
// ====================================================================
int psutil_init(void);
void psutil_cleanup(void);

// Error codes reported by psutil_last_error()
#define PSUTIL_OK 0
#define PSUTIL_ERROR_MEMORY -1
#define PSUTIL_ERROR_PERMISSION -2
#define PSUTIL_ERROR_NOT_FOUND -3
#define PSUTIL_ERROR_UNSUPPORTED -4
#define PSUTIL_ERROR_IO -5
#define PSUTIL_ERROR_INVALID_ARG -6

// Last error recorded by this library on the *calling thread*.
// Returns PSUTIL_OK if the last operation succeeded.
int psutil_last_error(void);

// Clear the per-thread error explicitly. The error is sticky: it is only
// overwritten by the next failing call, so read it right after the call you
// care about (a later successful call does not reset it).
void psutil_clear_error(void);

// Internal helper used by the platform backends; not part of the API.
void psutil_set_last_error(int err);

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

#ifdef __cplusplus
}
#endif

#endif // PSUTIL_H


