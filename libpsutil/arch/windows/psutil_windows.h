/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PSUTIL_WINDOWS_H
#define PSUTIL_WINDOWS_H

#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <wtsapi32.h>

// Define ANY_SIZE if not already defined
#ifndef ANY_SIZE
#define ANY_SIZE 1
#endif

// NT API definitions
typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// UNICODE_STRING structure
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

// KPRIORITY type
typedef LONG KPRIORITY;

// PROCESS_BASIC_INFORMATION structure
typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

// System information classes
typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    SystemProcessorInformation = 1,
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3,
    SystemPathInformation = 4,
    SystemProcessInformation = 5
} SYSTEM_INFORMATION_CLASS;

// Process information classes
typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessQuotaLimits = 1,
    ProcessIoCounters = 2,
    ProcessVmCounters = 3,
    ProcessTimes = 4
} PROCESSINFOCLASS;

// Object information classes
typedef enum _OBJECT_INFORMATION_CLASS {
    ObjectBasicInformation = 0,
    ObjectNameInformation = 1,
    ObjectTypeInformation = 2
} OBJECT_INFORMATION_CLASS;

// Memory information classes
typedef enum _MEMORY_INFORMATION_CLASS {
    MemoryBasicInformation = 0,
    MemoryWorkingSetInformation = 1,
    MemoryMappedFilenameInformation = 2
} MEMORY_INFORMATION_CLASS;

// WTS definitions - use system headers from wtsapi32.h

// Windows version constants
#define PSUTIL_WINDOWS_VISTA 60
#define PSUTIL_WINDOWS_7 61
#define PSUTIL_WINDOWS_8 62
#define PSUTIL_WINDOWS_8_1 63
#define PSUTIL_WINDOWS_10 100
#define PSUTIL_WINDOWS_NEW 999

// Global variables
extern int PSUTIL_WINVER;
extern SYSTEM_INFO PSUTIL_SYSTEM_INFO;

// Windows API functions
// These functions are already declared in Windows header files
// so we don't need to redeclare them here

// NT API functions
typedef NTSTATUS (WINAPI *PNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
typedef NTSTATUS (WINAPI *PNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
typedef NTSTATUS (WINAPI *PNtSetInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);
typedef NTSTATUS (WINAPI *PNtQueryObject)(HANDLE Handle, OBJECT_INFORMATION_CLASS ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength);
typedef NTSTATUS (WINAPI *PNtSuspendProcess)(HANDLE ProcessHandle);
typedef NTSTATUS (WINAPI *PNtResumeProcess)(HANDLE ProcessHandle);
typedef NTSTATUS (WINAPI *PNtQueryVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength);
typedef DWORD (WINAPI *PRtlNtStatusToDosErrorNoTeb)(NTSTATUS Status);
typedef DWORD (WINAPI *PRtlIpv4AddressToStringA)(const struct in_addr *Addr, PSTR S);
typedef DWORD (WINAPI *PRtlIpv6AddressToStringA)(const struct in6_addr *Addr, PSTR S);
typedef NTSTATUS (WINAPI *PRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);
typedef DWORD (WINAPI *PGetActiveProcessorCount)(WORD GroupNumber);
typedef BOOL (WINAPI *PGetLogicalProcessorInformationEx)(LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType, PVOID Buffer, PDWORD ReturnedLength);
typedef BOOL (WINAPI *PWTSEnumerateSessionsW)(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFO *ppSessionInfo, DWORD *pCount);
typedef BOOL (WINAPI *PWTSQuerySessionInformationW)(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR *ppBuffer, DWORD *pBytesReturned);
typedef VOID (WINAPI *PWTSFreeMemory)(PVOID pMemory);
typedef ULONGLONG (WINAPI *PGetTickCount64)(VOID);

// Global function pointers
extern PNtQuerySystemInformation g_NtQuerySystemInformation;
extern PNtQueryInformationProcess g_NtQueryInformationProcess;
extern PNtSetInformationProcess g_NtSetInformationProcess;
extern PNtQueryObject g_NtQueryObject;
extern PNtSuspendProcess g_NtSuspendProcess;
extern PNtResumeProcess g_NtResumeProcess;
extern PNtQueryVirtualMemory g_NtQueryVirtualMemory;
extern PRtlNtStatusToDosErrorNoTeb g_RtlNtStatusToDosErrorNoTeb;
extern PRtlIpv4AddressToStringA g_RtlIpv4AddressToStringA;
extern PRtlIpv6AddressToStringA g_RtlIpv6AddressToStringA;
extern PRtlGetVersion g_RtlGetVersion;
extern PGetActiveProcessorCount g_GetActiveProcessorCount;
extern PGetLogicalProcessorInformationEx g_GetLogicalProcessorInformationEx;
extern PWTSEnumerateSessionsW g_WTSEnumerateSessionsW;
extern PWTSQuerySessionInformationW g_WTSQuerySessionInformationW;
extern PWTSFreeMemory g_WTSFreeMemory;
extern PGetTickCount64 g_GetTickCount64;

// Helper functions
double psutil_FiletimeToUnixTime(FILETIME ft);
double psutil_LargeIntegerToUnixTime(LARGE_INTEGER li);
PVOID psutil_GetProcAddress(LPCSTR libname, LPCSTR procname);
PVOID psutil_GetProcAddressFromLib(LPCSTR libname, LPCSTR procname);
PVOID psutil_SetFromNTStatusErr(NTSTATUS Status, const char *syscall);
int psutil_loadlibs(void);
int psutil_set_winver(void);

// Library initialization
int psutil_windows_init(void);

// Process functions
int psutil_windows_pid_exists(DWORD pid);
uint32_t* psutil_windows_pids(int *count);
Process* psutil_windows_process_new(DWORD pid);
void psutil_windows_process_free(Process* proc);
DWORD psutil_windows_process_get_ppid(Process* proc);
const char* psutil_windows_process_get_name(Process* proc);
const char* psutil_windows_process_get_exe(Process* proc);
char** psutil_windows_process_get_cmdline(Process* proc, int* count);
int psutil_windows_process_get_status(Process* proc);
const char* psutil_windows_process_get_username(Process* proc);
double psutil_windows_process_get_create_time(Process* proc);
const char* psutil_windows_process_get_cwd(Process* proc);
int psutil_windows_process_get_nice(Process* proc);
int psutil_windows_process_set_nice(Process* proc, int value);
psutil_uids psutil_windows_process_get_uids(Process* proc);
psutil_gids psutil_windows_process_get_gids(Process* proc);
const char* psutil_windows_process_get_terminal(Process* proc);
int psutil_windows_process_get_num_fds(Process* proc);
psutil_io_counters psutil_windows_process_get_io_counters(Process* proc);
int psutil_windows_process_get_ionice(Process* proc);
int psutil_windows_process_set_ionice(Process* proc, int ioclass, int value);
int* psutil_windows_process_get_cpu_affinity(Process* proc, int* count);
int psutil_windows_process_set_cpu_affinity(Process* proc, int* cpus, int count);
int psutil_windows_process_get_cpu_num(Process* proc);
char** psutil_windows_process_get_environ(Process* proc, int* count);
int psutil_windows_process_get_num_handles(Process* proc);
psutil_ctx_switches psutil_windows_process_get_num_ctx_switches(Process* proc);
int psutil_windows_process_get_num_threads(Process* proc);
psutil_thread* psutil_windows_process_get_threads(Process* proc, int* count);
psutil_cpu_times psutil_windows_process_get_cpu_times(Process* proc);
psutil_memory_info psutil_windows_process_get_memory_info(Process* proc);
psutil_memory_info psutil_windows_process_get_memory_full_info(Process* proc);
double psutil_windows_process_get_memory_percent(Process* proc, const char* memtype);
psutil_memory_map* psutil_windows_process_get_memory_maps(Process* proc, int* count, int grouped);
psutil_open_file* psutil_windows_process_get_open_files(Process* proc, int* count);
psutil_net_connection* psutil_windows_process_get_net_connections(Process* proc, const char* kind, int* count);
int psutil_windows_process_send_signal(Process* proc, int sig);
int psutil_windows_process_suspend(Process* proc);
int psutil_windows_process_resume(Process* proc);
int psutil_windows_process_terminate(Process* proc);
int psutil_windows_process_kill(Process* proc);
int psutil_windows_process_wait(Process* proc, double timeout);
int psutil_windows_process_is_running(Process* proc);

// System functions
psutil_memory_info psutil_windows_virtual_memory(void);
psutil_memory_info psutil_windows_swap_memory(void);
psutil_cpu_times psutil_windows_cpu_times(int percpu);
double psutil_windows_cpu_percent(double interval, int percpu);
psutil_cpu_times psutil_windows_cpu_times_percent(double interval, int percpu);
int psutil_windows_cpu_count(int logical);
psutil_cpu_stats psutil_windows_cpu_stats(void);
psutil_io_counters psutil_windows_net_io_counters(int pernic);
psutil_net_connection* psutil_windows_net_connections(const char* kind, int* count);
psutil_net_if_addr* psutil_windows_net_if_addrs(int* count);
psutil_net_if_stat* psutil_windows_net_if_stats(int* count);
psutil_io_counters psutil_windows_disk_io_counters(int perdisk);
psutil_disk_partition* psutil_windows_disk_partitions(int all);
psutil_disk_usage psutil_windows_disk_usage(const char* path);
psutil_user* psutil_windows_users(int* count);
double psutil_windows_boot_time(void);

#endif // PSUTIL_WINDOWS_H
