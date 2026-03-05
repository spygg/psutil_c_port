/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <wtsapi32.h>
#include "../../psutil.h"
#include "psutil_windows.h"

// Global variables
int PSUTIL_WINVER;
SYSTEM_INFO PSUTIL_SYSTEM_INFO;

// Global function pointers
PNtQuerySystemInformation g_NtQuerySystemInformation;
PNtQueryInformationProcess g_NtQueryInformationProcess;
PNtSetInformationProcess g_NtSetInformationProcess;
PNtQueryObject g_NtQueryObject;
PNtSuspendProcess g_NtSuspendProcess;
PNtResumeProcess g_NtResumeProcess;
PNtQueryVirtualMemory g_NtQueryVirtualMemory;
PRtlNtStatusToDosErrorNoTeb g_RtlNtStatusToDosErrorNoTeb;
PRtlIpv4AddressToStringA g_RtlIpv4AddressToStringA;
PRtlIpv6AddressToStringA g_RtlIpv6AddressToStringA;
PRtlGetVersion g_RtlGetVersion;
PGetActiveProcessorCount g_GetActiveProcessorCount;
PGetLogicalProcessorInformationEx g_GetLogicalProcessorInformationEx;
PWTSEnumerateSessionsW g_WTSEnumerateSessionsW;
PWTSQuerySessionInformationW g_WTSQuerySessionInformationW;
PWTSFreeMemory g_WTSFreeMemory;
PGetTickCount64 g_GetTickCount64;

// Initialize the library
int psutil_windows_init(void) {
    // Load required libraries
    if (psutil_loadlibs() != 0) {
        return 1;
    }
    
    // Set Windows version
    psutil_set_winver();
    
    // Get system information
    GetSystemInfo(&PSUTIL_SYSTEM_INFO);
    
    // Print system information for debugging
    printf("System info: number of processors = %d\n", PSUTIL_SYSTEM_INFO.dwNumberOfProcessors);
    
    return 0;
}

// Helper functions
PVOID psutil_GetProcAddress(LPCSTR libname, LPCSTR procname) {
    HMODULE mod;
    FARPROC addr;

    if ((mod = GetModuleHandleA(libname)) == NULL) {
        return NULL;
    }
    if ((addr = GetProcAddress(mod, procname)) == NULL) {
        return NULL;
    }
    return addr;
}

PVOID psutil_GetProcAddressFromLib(LPCSTR libname, LPCSTR procname) {
    HMODULE mod;
    FARPROC addr;

    mod = LoadLibraryA(libname);
    if (mod == NULL) {
        return NULL;
    }
    if ((addr = GetProcAddress(mod, procname)) == NULL) {
        FreeLibrary(mod);
        return NULL;
    }
    return addr;
}

PVOID psutil_SetFromNTStatusErr(NTSTATUS Status, const char *syscall) {
    ULONG err;

    err = g_RtlNtStatusToDosErrorNoTeb(Status);
    SetLastError(err);
    return NULL;
}

int psutil_loadlibs(void) {
    // --- Mandatory
    g_NtQuerySystemInformation = psutil_GetProcAddressFromLib(
        "ntdll.dll", "NtQuerySystemInformation");
    if (!g_NtQuerySystemInformation)
        return 1;
    g_NtQueryInformationProcess = psutil_GetProcAddress(
        "ntdll.dll", "NtQueryInformationProcess");
    if (!g_NtQueryInformationProcess)
        return 1;
    g_NtSetInformationProcess = psutil_GetProcAddress(
        "ntdll.dll", "NtSetInformationProcess");
    if (!g_NtSetInformationProcess)
        return 1;
    g_NtQueryObject = psutil_GetProcAddressFromLib(
        "ntdll.dll", "NtQueryObject");
    if (!g_NtQueryObject)
        return 1;
    g_RtlIpv4AddressToStringA = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlIpv4AddressToStringA");
    if (!g_RtlIpv4AddressToStringA)
        return 1;
    g_RtlGetVersion = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlGetVersion");
    if (!g_RtlGetVersion)
        return 1;
    g_NtSuspendProcess = psutil_GetProcAddressFromLib(
        "ntdll", "NtSuspendProcess");
    if (!g_NtSuspendProcess)
        return 1;
    g_NtResumeProcess = psutil_GetProcAddressFromLib(
        "ntdll", "NtResumeProcess");
    if (!g_NtResumeProcess)
        return 1;
    g_NtQueryVirtualMemory = psutil_GetProcAddressFromLib(
        "ntdll", "NtQueryVirtualMemory");
    if (!g_NtQueryVirtualMemory)
        return 1;
    g_RtlNtStatusToDosErrorNoTeb = psutil_GetProcAddressFromLib(
        "ntdll", "RtlNtStatusToDosErrorNoTeb");
    if (!g_RtlNtStatusToDosErrorNoTeb)
        return 1;
    g_GetTickCount64 = psutil_GetProcAddress(
        "kernel32", "GetTickCount64");
    if (!g_GetTickCount64)
        return 1;
    g_RtlIpv6AddressToStringA = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlIpv6AddressToStringA");
    if (!g_RtlIpv6AddressToStringA)
        return 1;

    // --- Optional
    // minimum requirement: Win 7
    g_GetActiveProcessorCount = psutil_GetProcAddress(
        "kernel32", "GetActiveProcessorCount");
    // minimum requirement: Win 7
    g_GetLogicalProcessorInformationEx = psutil_GetProcAddressFromLib(
        "kernel32", "GetLogicalProcessorInformationEx");
    // minimum requirements: Windows Server Core
    g_WTSEnumerateSessionsW = psutil_GetProcAddressFromLib(
        "wtsapi32.dll", "WTSEnumerateSessionsW");
    g_WTSQuerySessionInformationW = psutil_GetProcAddressFromLib(
        "wtsapi32.dll", "WTSQuerySessionInformationW");
    g_WTSFreeMemory = psutil_GetProcAddressFromLib(
        "wtsapi32.dll", "WTSFreeMemory");

    return 0;
}

int psutil_set_winver() {
    RTL_OSVERSIONINFOEXW versionInfo;
    ULONG maj;
    ULONG min;

    versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
    memset(&versionInfo, 0, sizeof(RTL_OSVERSIONINFOEXW));
    g_RtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo);
    maj = versionInfo.dwMajorVersion;
    min = versionInfo.dwMinorVersion;
    if (maj == 6 && min == 0)
        PSUTIL_WINVER = PSUTIL_WINDOWS_VISTA;  // or Server 2008
    else if (maj == 6 && min == 1)
        PSUTIL_WINVER = PSUTIL_WINDOWS_7;
    else if (maj == 6 && min == 2)
        PSUTIL_WINVER = PSUTIL_WINDOWS_8;
    else if (maj == 6 && min == 3)
        PSUTIL_WINVER = PSUTIL_WINDOWS_8_1;
    else if (maj == 10 && min == 0)
        PSUTIL_WINVER = PSUTIL_WINDOWS_10;
    else
        PSUTIL_WINVER = PSUTIL_WINDOWS_NEW;
    return 0;
}

double psutil_FiletimeToUnixTime(FILETIME ft) {
    ULONGLONG ret;

    // 100 nanosecond intervals since January 1, 1601.
    ret = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    // Change starting time to the Epoch (00:00:00 UTC, January 1, 1970).
    ret -= 116444736000000000ull;
    // Convert nano secs to secs.
    return (double)ret / 10000000ull;
}

double psutil_LargeIntegerToUnixTime(LARGE_INTEGER li) {
    ULONGLONG ret;

    // 100 nanosecond intervals since January 1, 1601.
    ret = ((ULONGLONG)li.HighPart << 32) | li.LowPart;
    // Change starting time to the Epoch (00:00:00 UTC, January 1, 1970).
    ret -= 116444736000000000ull;
    // Convert nano secs to secs.
    return (double)ret / 10000000ull;
}

// Process functions
int psutil_windows_pid_exists(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess != NULL) {
        CloseHandle(hProcess);
        return 1;
    }
    return 0;
}

uint32_t* psutil_windows_pids(int *count) {
    DWORD *pids = NULL;
    DWORD size = 1024;
    DWORD needed;

    pids = (DWORD*)malloc(size * sizeof(DWORD));
    if (pids == NULL) {
        *count = 0;
        return NULL;
    }

    if (!EnumProcesses(pids, size * sizeof(DWORD), &needed)) {
        free(pids);
        *count = 0;
        return NULL;
    }

    *count = needed / sizeof(DWORD);
    return (uint32_t*)pids;
}

Process* psutil_windows_process_new(DWORD pid) {
    Process* proc = (Process*)malloc(sizeof(Process));
    if (proc == NULL) {
        return NULL;
    }

    // If pid is 0, use current process PID
    if (pid == 0) {
        pid = GetCurrentProcessId();
    }

    proc->pid = pid;
    proc->name = NULL;
    proc->exe = NULL;
    proc->cwd = NULL;
    proc->username = NULL;
    proc->create_time = 0.0;
    proc->ppid = 0;
    proc->status = STATUS_RUNNING;

    // Try to get process information immediately
    psutil_windows_process_get_name(proc);
    psutil_windows_process_get_exe(proc);
    psutil_windows_process_get_username(proc);
    psutil_windows_process_get_create_time(proc);
    psutil_windows_process_get_cwd(proc);

    return proc;
}

void psutil_windows_process_free(Process* proc) {
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

DWORD psutil_windows_process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    if (proc->ppid > 0) {
        return proc->ppid;
    }
    
    // For now, return a placeholder value
    proc->ppid = 1;
    return proc->ppid;
}

const char* psutil_windows_process_get_name(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->name != NULL) {
        return proc->name;
    }
    
    // Try with full permissions first
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        // Fallback to limited permissions
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
        if (hProcess == NULL) {
            return NULL;
        }
    }
    
    char name[MAX_PATH];
    DWORD size = MAX_PATH;
    if (GetModuleBaseNameA(hProcess, NULL, name, size) > 0) {
        proc->name = strdup(name);
    }
    
    CloseHandle(hProcess);
    return proc->name;
}

const char* psutil_windows_process_get_exe(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->exe != NULL) {
        return proc->exe;
    }
    
    // Try with full permissions first
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        // Fallback to limited permissions
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
        if (hProcess == NULL) {
            return NULL;
        }
    }
    
    char exe[MAX_PATH];
    DWORD size = MAX_PATH;
    if (GetModuleFileNameExA(hProcess, NULL, exe, size) > 0) {
        proc->exe = strdup(exe);
    }
    
    CloseHandle(hProcess);
    return proc->exe;
}

char** psutil_windows_process_get_cmdline(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_windows_process_get_status(Process* proc) {
    // TODO: Implement
    return STATUS_RUNNING;
}

const char* psutil_windows_process_get_username(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->username != NULL) {
        return proc->username;
    }
    
    // Try with full permissions first
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        // Fallback to limited permissions
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
        if (hProcess == NULL) {
            return NULL;
        }
    }
    
    HANDLE hToken;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        CloseHandle(hProcess);
        return NULL;
    }
    
    DWORD size = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &size);
    if (size > 0) {
        PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(size);
        if (pTokenUser != NULL) {
            if (GetTokenInformation(hToken, TokenUser, pTokenUser, size, &size)) {
                char username[256];
                char domain[256];
                DWORD usernameSize = sizeof(username);
                DWORD domainSize = sizeof(domain);
                SID_NAME_USE sidType;
                
                if (LookupAccountSidA(NULL, pTokenUser->User.Sid, username, &usernameSize, domain, &domainSize, &sidType)) {
                    char fullUsername[512];
                    snprintf(fullUsername, sizeof(fullUsername), "%s\\%s", domain, username);
                    proc->username = strdup(fullUsername);
                }
            }
            free(pTokenUser);
        }
    }
    
    CloseHandle(hToken);
    CloseHandle(hProcess);
    return proc->username;
}

double psutil_windows_process_get_create_time(Process* proc) {
    if (proc == NULL) {
        return 0.0;
    }
    
    if (proc->create_time > 0.0) {
        return proc->create_time;
    }
    
    // Try with full permissions first
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        // Fallback to limited permissions
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
        if (hProcess == NULL) {
            return 0.0;
        }
    }
    
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        proc->create_time = psutil_FiletimeToUnixTime(createTime);
    }
    
    CloseHandle(hProcess);
    return proc->create_time;
}

const char* psutil_windows_process_get_cwd(Process* proc) {
    if (proc == NULL) {
        return NULL;
    }
    
    if (proc->cwd != NULL) {
        return proc->cwd;
    }
    
    // This is a placeholder, getting CWD for another process is more complex
    // We'll just return the current directory for now
    char cwd[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, cwd) > 0) {
        proc->cwd = strdup(cwd);
    }
    
    return proc->cwd;
}

int psutil_windows_process_get_nice(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_windows_process_set_nice(Process* proc, int value) {
    // TODO: Implement
    return 0;
}

psutil_uids psutil_windows_process_get_uids(Process* proc) {
    psutil_uids uids = {0};
    // TODO: Implement
    return uids;
}

psutil_gids psutil_windows_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    // TODO: Implement
    return gids;
}

const char* psutil_windows_process_get_terminal(Process* proc) {
    // TODO: Implement
    return NULL;
}

int psutil_windows_process_get_num_fds(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_io_counters psutil_windows_process_get_io_counters(Process* proc) {
    psutil_io_counters counters = {0};
    if (proc == NULL) {
        return counters;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return counters;
    }
    
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(hProcess, &ioCounters)) {
        counters.read_count = ioCounters.ReadOperationCount;
        counters.write_count = ioCounters.WriteOperationCount;
        counters.read_bytes = ioCounters.ReadTransferCount;
        counters.write_bytes = ioCounters.WriteTransferCount;
    }
    
    CloseHandle(hProcess);
    return counters;
}

int psutil_windows_process_get_ionice(Process* proc) {
    // TODO: Implement
    return 0;
}

int psutil_windows_process_set_ionice(Process* proc, int ioclass, int value) {
    // TODO: Implement
    return 0;
}

int* psutil_windows_process_get_cpu_affinity(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_windows_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    // TODO: Implement
    return 0;
}

int psutil_windows_process_get_cpu_num(Process* proc) {
    // TODO: Implement
    return 0;
}

char** psutil_windows_process_get_environ(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_windows_process_get_num_handles(Process* proc) {
    // TODO: Implement
    return 0;
}

psutil_ctx_switches psutil_windows_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    // TODO: Implement
    return switches;
}

int psutil_windows_process_get_num_threads(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    // Use toolhelp32 to count threads
    int num_threads = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == proc->pid) {
                    num_threads++;
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
    
    return num_threads > 0 ? num_threads : 1;
}

psutil_thread* psutil_windows_process_get_threads(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_cpu_times psutil_windows_process_get_cpu_times(Process* proc) {
    psutil_cpu_times times = {0};
    if (proc == NULL) {
        return times;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return times;
    }
    
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        times.user = psutil_FiletimeToUnixTime(userTime);
        times.system = psutil_FiletimeToUnixTime(kernelTime);
    }
    
    CloseHandle(hProcess);
    return times;
}

psutil_memory_info psutil_windows_process_get_memory_info(Process* proc) {
    psutil_memory_info info = {0};
    if (proc == NULL) {
        return info;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        return info;
    }
    
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        info.rss = pmc.WorkingSetSize;
        info.vms = pmc.PagefileUsage;
        info.shared = 0;
        info.text = 0;
        info.lib = 0;
        info.data = 0;
        info.dirty = 0;
        info.uss = 0;
        info.pss = 0;
    }
    
    CloseHandle(hProcess);
    return info;
}

psutil_memory_info psutil_windows_process_get_memory_full_info(Process* proc) {
    psutil_memory_info info = {0};
    // TODO: Implement
    return info;
}

double psutil_windows_process_get_memory_percent(Process* proc, const char* memtype) {
    // TODO: Implement
    return 0.0;
}

psutil_memory_map* psutil_windows_process_get_memory_maps(Process* proc, int* count, int grouped) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_open_file* psutil_windows_process_get_open_files(Process* proc, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

psutil_net_connection* psutil_windows_process_get_net_connections(Process* proc, const char* kind, int* count) {
    // TODO: Implement
    *count = 0;
    return NULL;
}

int psutil_windows_process_send_signal(Process* proc, int sig) {
    // TODO: Implement
    return 0;
}

int psutil_windows_process_suspend(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    if (g_NtSuspendProcess != NULL) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, proc->pid);
        if (hProcess == NULL) {
            return -1;
        }
        
        NTSTATUS status = g_NtSuspendProcess(hProcess);
        CloseHandle(hProcess);
        
        return (status == STATUS_SUCCESS) ? 0 : -1;
    }
    
    return -1;
}

int psutil_windows_process_resume(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    if (g_NtResumeProcess != NULL) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, proc->pid);
        if (hProcess == NULL) {
            return -1;
        }
        
        NTSTATUS status = g_NtResumeProcess(hProcess);
        CloseHandle(hProcess);
        
        return (status == STATUS_SUCCESS) ? 0 : -1;
    }
    
    return -1;
}

int psutil_windows_process_terminate(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, proc->pid);
    if (hProcess == NULL) {
        return -1;
    }
    
    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    
    return result ? 0 : -1;
}

int psutil_windows_process_kill(Process* proc) {
    // On Windows, kill is the same as terminate
    return psutil_windows_process_terminate(proc);
}

int psutil_windows_process_wait(Process* proc, double timeout) {
    if (proc == NULL) {
        return -1;
    }
    
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, proc->pid);
    if (hProcess == NULL) {
        return -1;
    }
    
    DWORD timeoutMs = (timeout < 0) ? INFINITE : (DWORD)(timeout * 1000);
    DWORD result = WaitForSingleObject(hProcess, timeoutMs);
    CloseHandle(hProcess);
    
    if (result == WAIT_OBJECT_0) {
        return 0;  // Process exited
    } else if (result == WAIT_TIMEOUT) {
        return 1;  // Timeout
    } else {
        return -1; // Error
    }
}

int psutil_windows_process_is_running(Process* proc) {
    if (proc == NULL) {
        return 0;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        // Process doesn't exist or we don't have access
        return (GetLastError() == ERROR_ACCESS_DENIED) ? 1 : 0;
    }
    
    DWORD exitCode;
    BOOL result = GetExitCodeProcess(hProcess, &exitCode);
    CloseHandle(hProcess);
    
    if (!result) {
        return 0;
    }
    
    return (exitCode == STILL_ACTIVE) ? 1 : 0;
}

// System functions
psutil_memory_info psutil_windows_virtual_memory(void) {
    psutil_memory_info info = {0};
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info.rss = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        info.vms = memInfo.ullTotalPhys;
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

psutil_memory_info psutil_windows_swap_memory(void) {
    psutil_memory_info info = {0};
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info.rss = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
        info.vms = memInfo.ullTotalPageFile;
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

psutil_cpu_times psutil_windows_cpu_times(int percpu) {
    psutil_cpu_times times = {0};
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        times.user = psutil_FiletimeToUnixTime(userTime);
        times.system = psutil_FiletimeToUnixTime(kernelTime) - times.user;
    }
    return times;
}

double psutil_windows_cpu_percent(double interval, int percpu) {
    // TODO: Implement
    return 0.0;
}

psutil_cpu_times psutil_windows_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    // TODO: Implement
    return times;
}

int psutil_windows_cpu_count(int logical) {
    if (logical) {
        return PSUTIL_SYSTEM_INFO.dwNumberOfProcessors;
    }
    // For physical core count, we need to use GetLogicalProcessorInformationEx
    // For simplicity, return the same as logical for now
    return PSUTIL_SYSTEM_INFO.dwNumberOfProcessors;
}

psutil_cpu_stats psutil_windows_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    // TODO: Implement
    return stats;
}

psutil_io_counters psutil_windows_net_io_counters(int pernic) {
    psutil_io_counters counters = {0};
    
    // Use the older GetIfTable API for compatibility
    ULONG ulOutBufLen = 0;
    DWORD dwRetVal = GetIfTable(NULL, &ulOutBufLen, FALSE);
    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return counters;
    }
    
    MIB_IFTABLE* pIfTable = (MIB_IFTABLE*)malloc(ulOutBufLen);
    if (pIfTable == NULL) {
        return counters;
    }
    
    if (GetIfTable(pIfTable, &ulOutBufLen, FALSE) != NO_ERROR) {
        free(pIfTable);
        return counters;
    }
    
    for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
        MIB_IFROW* row = &pIfTable->table[i];
        
        // Skip loopback
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        
        // Only count operational interfaces
        if (row->dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
            counters.read_bytes += row->dwInOctets;
            counters.write_bytes += row->dwOutOctets;
            counters.read_count += row->dwInUcastPkts + row->dwInNUcastPkts;
            counters.write_count += row->dwOutUcastPkts + row->dwOutNUcastPkts;
        }
    }
    
    free(pIfTable);
    return counters;
}

// Helper function to convert TCP state to psutil status
static int tcp_state_to_status(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED:     return CONN_CLOSE;
        case MIB_TCP_STATE_LISTEN:     return CONN_LISTEN;
        case MIB_TCP_STATE_SYN_SENT:   return CONN_SYN_SENT;
        case MIB_TCP_STATE_SYN_RCVD:   return CONN_SYN_RECV;
        case MIB_TCP_STATE_ESTAB:      return CONN_ESTABLISHED;
        case MIB_TCP_STATE_FIN_WAIT1:  return CONN_FIN_WAIT1;
        case MIB_TCP_STATE_FIN_WAIT2:  return CONN_FIN_WAIT2;
        case MIB_TCP_STATE_CLOSE_WAIT: return CONN_CLOSE_WAIT;
        case MIB_TCP_STATE_CLOSING:    return CONN_CLOSING;
        case MIB_TCP_STATE_LAST_ACK:   return CONN_LAST_ACK;
        case MIB_TCP_STATE_TIME_WAIT:  return CONN_TIME_WAIT;
        default:                       return CONN_NONE;
    }
}

// Helper function to format address
static void format_address(char* buf, size_t buf_size, DWORD addr, DWORD port) {
    struct in_addr inaddr;
    inaddr.s_addr = addr;
    snprintf(buf, buf_size, "%s:%d", inet_ntoa(inaddr), ntohs((u_short)port));
}

// Helper function to format IPv6 address
static void format_address6(char* buf, size_t buf_size, const UCHAR* addr, DWORD port) {
    char addr_str[INET6_ADDRSTRLEN];
    InetNtopA(AF_INET6, addr, addr_str, sizeof(addr_str));
    snprintf(buf, buf_size, "[%s]:%d", addr_str, ntohs((u_short)port));
}

psutil_net_connection* psutil_windows_net_connections(const char* kind, int* count) {
    *count = 0;
    
    // Determine what types of connections to retrieve
    int get_tcp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp4") == 0 || strcmp(kind, "tcp") == 0);
    int get_tcp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "tcp6") == 0 || strcmp(kind, "tcp") == 0);
    int get_udp = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp4") == 0 || strcmp(kind, "udp") == 0);
    int get_udp6 = (kind == NULL || strcmp(kind, "inet") == 0 || strcmp(kind, "udp6") == 0 || strcmp(kind, "udp") == 0);
    
    int capacity = 64;
    int index = 0;
    psutil_net_connection* connections = (psutil_net_connection*)malloc(capacity * sizeof(psutil_net_connection));
    if (connections == NULL) {
        return NULL;
    }
    
    // Get TCP IPv4 connections
    if (get_tcp) {
        PMIB_TCPTABLE_OWNER_PID tcp_table = NULL;
        DWORD size = 0;
        
        // Get required size
        GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        tcp_table = (PMIB_TCPTABLE_OWNER_PID)malloc(size);
        
        if (tcp_table != NULL && GetExtendedTcpTable(tcp_table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < tcp_table->dwNumEntries; i++) {
                if (index >= capacity) {
                    capacity *= 2;
                    psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                    if (new_conn == NULL) {
                        free(tcp_table);
                        free(connections);
                        *count = 0;
                        return NULL;
                    }
                    connections = new_conn;
                }
                
                connections[index].fd = -1;
                connections[index].family = AF_INET;
                connections[index].type = SOCK_STREAM;
                format_address(connections[index].laddr, sizeof(connections[index].laddr), 
                              tcp_table->table[i].dwLocalAddr, tcp_table->table[i].dwLocalPort);
                format_address(connections[index].raddr, sizeof(connections[index].raddr), 
                              tcp_table->table[i].dwRemoteAddr, tcp_table->table[i].dwRemotePort);
                connections[index].status = tcp_state_to_status(tcp_table->table[i].dwState);
                index++;
            }
        }
        free(tcp_table);
    }
    
    // Get TCP IPv6 connections
    if (get_tcp6) {
        PMIB_TCP6TABLE_OWNER_PID tcp6_table = NULL;
        DWORD size = 0;
        
        GetExtendedTcpTable(NULL, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
        tcp6_table = (PMIB_TCP6TABLE_OWNER_PID)malloc(size);
        
        if (tcp6_table != NULL && GetExtendedTcpTable(tcp6_table, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < tcp6_table->dwNumEntries; i++) {
                if (index >= capacity) {
                    capacity *= 2;
                    psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                    if (new_conn == NULL) {
                        free(tcp6_table);
                        free(connections);
                        *count = 0;
                        return NULL;
                    }
                    connections = new_conn;
                }
                
                connections[index].fd = -1;
                connections[index].family = AF_INET6;
                connections[index].type = SOCK_STREAM;
                format_address6(connections[index].laddr, sizeof(connections[index].laddr), 
                               tcp6_table->table[i].ucLocalAddr, tcp6_table->table[i].dwLocalPort);
                format_address6(connections[index].raddr, sizeof(connections[index].raddr), 
                               tcp6_table->table[i].ucRemoteAddr, tcp6_table->table[i].dwRemotePort);
                connections[index].status = tcp_state_to_status(tcp6_table->table[i].dwState);
                index++;
            }
        }
        free(tcp6_table);
    }
    
    // Get UDP IPv4 connections
    if (get_udp) {
        PMIB_UDPTABLE_OWNER_PID udp_table = NULL;
        DWORD size = 0;
        
        GetExtendedUdpTable(NULL, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        udp_table = (PMIB_UDPTABLE_OWNER_PID)malloc(size);
        
        if (udp_table != NULL && GetExtendedUdpTable(udp_table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            for (DWORD i = 0; i < udp_table->dwNumEntries; i++) {
                if (index >= capacity) {
                    capacity *= 2;
                    psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                    if (new_conn == NULL) {
                        free(udp_table);
                        free(connections);
                        *count = 0;
                        return NULL;
                    }
                    connections = new_conn;
                }
                
                connections[index].fd = -1;
                connections[index].family = AF_INET;
                connections[index].type = SOCK_DGRAM;
                format_address(connections[index].laddr, sizeof(connections[index].laddr), 
                              udp_table->table[i].dwLocalAddr, udp_table->table[i].dwLocalPort);
                connections[index].raddr[0] = '\0';
                connections[index].status = CONN_NONE;
                index++;
            }
        }
        free(udp_table);
    }
    
    // Get UDP IPv6 connections
    if (get_udp6) {
        PMIB_UDP6TABLE_OWNER_PID udp6_table = NULL;
        DWORD size = 0;
        
        GetExtendedUdpTable(NULL, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
        udp6_table = (PMIB_UDP6TABLE_OWNER_PID)malloc(size);
        
        if (udp6_table != NULL && GetExtendedUdpTable(udp6_table, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
            for (DWORD i = 0; i < udp6_table->dwNumEntries; i++) {
                if (index >= capacity) {
                    capacity *= 2;
                    psutil_net_connection* new_conn = (psutil_net_connection*)realloc(connections, capacity * sizeof(psutil_net_connection));
                    if (new_conn == NULL) {
                        free(udp6_table);
                        free(connections);
                        *count = 0;
                        return NULL;
                    }
                    connections = new_conn;
                }
                
                connections[index].fd = -1;
                connections[index].family = AF_INET6;
                connections[index].type = SOCK_DGRAM;
                format_address6(connections[index].laddr, sizeof(connections[index].laddr), 
                               udp6_table->table[i].ucLocalAddr, udp6_table->table[i].dwLocalPort);
                connections[index].raddr[0] = '\0';
                connections[index].status = CONN_NONE;
                index++;
            }
        }
        free(udp6_table);
    }
    
    *count = index;
    if (index == 0) {
        free(connections);
        return NULL;
    }
    return connections;
}

psutil_net_if_addr* psutil_windows_net_if_addrs(int* count) {
    *count = 0;
    
    // Get adapter addresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_UNSPEC;
    PIP_ADAPTER_ADDRESSES adapterAddresses = NULL;
    ULONG bufferSize = 0;
    
    // First call to get buffer size
    GetAdaptersAddresses(family, flags, NULL, NULL, &bufferSize);
    
    adapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
    if (adapterAddresses == NULL) {
        return NULL;
    }
    
    DWORD result = GetAdaptersAddresses(family, flags, NULL, adapterAddresses, &bufferSize);
    if (result != ERROR_SUCCESS) {
        free(adapterAddresses);
        return NULL;
    }
    
    // Count addresses
    int capacity = 0;
    PIP_ADAPTER_ADDRESSES adapter = adapterAddresses;
    while (adapter != NULL) {
        PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
        while (unicast != NULL) {
            capacity++;
            unicast = unicast->Next;
        }
        adapter = adapter->Next;
    }
    
    if (capacity == 0) {
        free(adapterAddresses);
        return NULL;
    }
    
    psutil_net_if_addr* addrs = (psutil_net_if_addr*)malloc(capacity * sizeof(psutil_net_if_addr));
    if (addrs == NULL) {
        free(adapterAddresses);
        return NULL;
    }
    
    int index = 0;
    adapter = adapterAddresses;
    while (adapter != NULL && index < capacity) {
        // Get interface name
        char ifName[64] = {0};
        WideCharToMultiByte(CP_ACP, 0, adapter->FriendlyName, -1, ifName, sizeof(ifName), NULL, NULL);
        
        PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
        while (unicast != NULL && index < capacity) {
            strncpy(addrs[index].name, ifName, sizeof(addrs[index].name) - 1);
            
            SOCKET_ADDRESS* sockAddr = &unicast->Address;
            if (sockAddr->lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in* sin = (struct sockaddr_in*)sockAddr->lpSockaddr;
                strncpy(addrs[index].address, inet_ntoa(sin->sin_addr), sizeof(addrs[index].address) - 1);
                
                // Get netmask
                ULONG mask;
                ConvertLengthToIpv4Mask(unicast->OnLinkPrefixLength, &mask);
                struct in_addr maskAddr;
                maskAddr.s_addr = mask;
                strncpy(addrs[index].netmask, inet_ntoa(maskAddr), sizeof(addrs[index].netmask) - 1);
                
                // Get broadcast (approximation)
                strncpy(addrs[index].broadcast, "255.255.255.255", sizeof(addrs[index].broadcast) - 1);
            } else if (sockAddr->lpSockaddr->sa_family == AF_INET6) {
                struct sockaddr_in6* sin6 = (struct sockaddr_in6*)sockAddr->lpSockaddr;
                char addrStr[INET6_ADDRSTRLEN];
                InetNtopA(AF_INET6, &sin6->sin6_addr, addrStr, sizeof(addrStr));
                strncpy(addrs[index].address, addrStr, sizeof(addrs[index].address) - 1);
                strncpy(addrs[index].netmask, "", sizeof(addrs[index].netmask) - 1);
                strncpy(addrs[index].broadcast, "", sizeof(addrs[index].broadcast) - 1);
            }
            
            index++;
            unicast = unicast->Next;
        }
        adapter = adapter->Next;
    }
    
    free(adapterAddresses);
    *count = index;
    
    if (index == 0) {
        free(addrs);
        return NULL;
    }
    
    return addrs;
}

psutil_net_if_stat* psutil_windows_net_if_stats(int* count) {
    *count = 0;
    
    // Use the older GetIfTable API for compatibility
    ULONG ulOutBufLen = 0;
    DWORD dwRetVal = GetIfTable(NULL, &ulOutBufLen, FALSE);
    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return NULL;
    }
    
    MIB_IFTABLE* pIfTable = (MIB_IFTABLE*)malloc(ulOutBufLen);
    if (pIfTable == NULL) {
        return NULL;
    }
    
    if (GetIfTable(pIfTable, &ulOutBufLen, FALSE) != NO_ERROR) {
        free(pIfTable);
        return NULL;
    }
    
    int capacity = pIfTable->dwNumEntries;
    psutil_net_if_stat* stats = (psutil_net_if_stat*)malloc(capacity * sizeof(psutil_net_if_stat));
    if (stats == NULL) {
        free(pIfTable);
        return NULL;
    }
    
    int index = 0;
    for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
        MIB_IFROW* row = &pIfTable->table[i];
        
        // Skip loopback
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        
        // Get interface name
        char ifName[64] = {0};
        WideCharToMultiByte(CP_ACP, 0, row->wszName, -1, ifName, sizeof(ifName), NULL, NULL);
        strncpy(stats[index].name, ifName, sizeof(stats[index].name) - 1);
        
        stats[index].isup = (row->dwOperStatus == IF_OPER_STATUS_OPERATIONAL) ? 1 : 0;
        stats[index].speed = (int)(row->dwSpeed / 1000000); // Convert to Mbps
        stats[index].mtu = row->dwMtu;
        
        // Determine duplex (not available in MIB_IFROW, use unknown)
        stats[index].duplex = NIC_DUPLEX_UNKNOWN;
        
        index++;
    }
    
    free(pIfTable);
    *count = index;
    
    if (index == 0) {
        free(stats);
        return NULL;
    }
    
    return stats;
}

psutil_io_counters psutil_windows_disk_io_counters(int perdisk) {
    psutil_io_counters counters = {0};

    // Try to get disk performance using DeviceIoControl
    HANDLE hDevice = CreateFileA("\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        DISK_PERFORMANCE diskPerformance;
        DWORD bytesReturned;
        if (DeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0,
                           &diskPerformance, sizeof(diskPerformance), &bytesReturned, NULL)) {
            counters.read_bytes = diskPerformance.BytesRead.QuadPart;
            counters.write_bytes = diskPerformance.BytesWritten.QuadPart;
            counters.read_count = diskPerformance.ReadCount;
            counters.write_count = diskPerformance.WriteCount;
        }
        CloseHandle(hDevice);
    }

    return counters;
}

psutil_disk_partition* psutil_windows_disk_partitions(int all) {
    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        return NULL;
    }

    // Count partitions first
    int count = 0;
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char drivePath[4] = {'A' + i, ':', '\\', '\0'};
            UINT driveType = GetDriveTypeA(drivePath);
            if (all || driveType == DRIVE_FIXED || driveType == DRIVE_REMOTE) {
                count++;
            }
        }
    }

    if (count == 0) {
        return NULL;
    }

    // Allocate array (extra element for terminator)
    psutil_disk_partition* partitions = (psutil_disk_partition*)malloc((count + 1) * sizeof(psutil_disk_partition));
    if (partitions == NULL) {
        return NULL;
    }

    int index = 0;
    for (int i = 0; i < 26 && index < count; i++) {
        if (drives & (1 << i)) {
            char drivePath[4] = {'A' + i, ':', '\\', '\0'};
            char mountPoint[4] = {'A' + i, ':', '\\', '\0'};
            UINT driveType = GetDriveTypeA(drivePath);

            if (all || driveType == DRIVE_FIXED || driveType == DRIVE_REMOTE) {
                // Get filesystem type
                char fsName[64] = {0};
                GetVolumeInformationA(drivePath, NULL, 0, NULL, NULL, NULL, fsName, sizeof(fsName));

                // Get device name (volume path)
                char deviceName[256] = {0};
                char volumePath[4] = {'A' + i, ':', '\0', '\0'};
                DWORD retLen = QueryDosDeviceA(volumePath, deviceName, sizeof(deviceName));
                if (retLen == 0) {
                    strncpy(deviceName, drivePath, sizeof(deviceName) - 1);
                }

                // Build opts string
                char opts[256] = {0};
                switch (driveType) {
                    case DRIVE_FIXED:
                        strncpy(opts, "rw,fixed", sizeof(opts) - 1);
                        break;
                    case DRIVE_REMOTE:
                        strncpy(opts, "rw,remote", sizeof(opts) - 1);
                        break;
                    case DRIVE_CDROM:
                        strncpy(opts, "ro,cdrom", sizeof(opts) - 1);
                        break;
                    case DRIVE_REMOVABLE:
                        strncpy(opts, "rw,removable", sizeof(opts) - 1);
                        break;
                    default:
                        strncpy(opts, "rw", sizeof(opts) - 1);
                        break;
                }

                strncpy(partitions[index].device, deviceName, sizeof(partitions[index].device) - 1);
                strncpy(partitions[index].mountpoint, mountPoint, sizeof(partitions[index].mountpoint) - 1);
                strncpy(partitions[index].fstype, fsName[0] ? fsName : "unknown", sizeof(partitions[index].fstype) - 1);
                strncpy(partitions[index].opts, opts, sizeof(partitions[index].opts) - 1);
                index++;
            }
        }
    }

    // Mark end of array
    if (index < count + 1) {
        partitions[index].device[0] = '\0';
    }

    return partitions;
}

psutil_disk_usage psutil_windows_disk_usage(const char* path) {
    psutil_disk_usage usage = {0};
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        usage.total = totalNumberOfBytes.QuadPart;
        usage.free = totalNumberOfFreeBytes.QuadPart;
        usage.used = usage.total - usage.free;
        usage.percent = (double)usage.used / usage.total * 100.0;
    }
    return usage;
}

psutil_user* psutil_windows_users(int* count) {
    *count = 0;
    
    if (g_WTSEnumerateSessionsW == NULL || g_WTSQuerySessionInformationW == NULL || g_WTSFreeMemory == NULL) {
        return NULL;
    }
    
    PWTS_SESSION_INFO sessions = NULL;
    DWORD sessionCount = 0;
    
    if (!g_WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &sessionCount)) {
        return NULL;
    }
    
    if (sessionCount == 0) {
        g_WTSFreeMemory(sessions);
        return NULL;
    }
    
    psutil_user* users = (psutil_user*)malloc(sessionCount * sizeof(psutil_user));
    if (users == NULL) {
        g_WTSFreeMemory(sessions);
        return NULL;
    }
    
    int index = 0;
    for (DWORD i = 0; i < sessionCount; i++) {
        // Skip services and listener sessions
        if (sessions[i].State != WTSActive && sessions[i].State != WTSConnected) {
            continue;
        }
        
        LPWSTR username = NULL;
        LPWSTR domain = NULL;
        DWORD bytesReturned;
        
        // Get username
        if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSUserName, &username, &bytesReturned)) {
            if (username != NULL && wcslen(username) > 0) {
                // Convert username to char
                WideCharToMultiByte(CP_ACP, 0, username, -1, users[index].name, sizeof(users[index].name), NULL, NULL);
                
                // Get domain
                if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSDomainName, &domain, &bytesReturned)) {
                    if (domain != NULL && wcslen(domain) > 0) {
                        char domainStr[64] = {0};
                        WideCharToMultiByte(CP_ACP, 0, domain, -1, domainStr, sizeof(domainStr), NULL, NULL);
                        // Prepend domain to username
                        char fullName[128];
                        snprintf(fullName, sizeof(fullName), "%s\\%s", domainStr, users[index].name);
                        strncpy(users[index].name, fullName, sizeof(users[index].name) - 1);
                    }
                    g_WTSFreeMemory(domain);
                }
                
                // Get terminal name
                LPWSTR stationName = NULL;
                if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSWinStationName, &stationName, &bytesReturned)) {
                    if (stationName != NULL) {
                        WideCharToMultiByte(CP_ACP, 0, stationName, -1, users[index].terminal, sizeof(users[index].terminal), NULL, NULL);
                        g_WTSFreeMemory(stationName);
                    }
                }
                
                // Get logon time (use current time as approximation)
                users[index].started = time(NULL);
                
                index++;
            }
            g_WTSFreeMemory(username);
        }
    }
    
    g_WTSFreeMemory(sessions);
    *count = index;
    
    if (index == 0) {
        free(users);
        return NULL;
    }
    
    return users;
}

double psutil_windows_boot_time(void) {
    FILETIME ftSystemTime, ftLocalTime;
    SYSTEMTIME st;
    ULARGE_INTEGER uli;
    
    // Get system time as FILETIME
    GetSystemTimeAsFileTime(&ftSystemTime);
    
    // Convert to local time
    FileTimeToLocalFileTime(&ftSystemTime, &ftLocalTime);
    
    // Convert to SYSTEMTIME
    FileTimeToSystemTime(&ftLocalTime, &st);
    
    // Get tick count
    DWORD dwTickCount = GetTickCount();
    
    // Calculate boot time
    double currentTime = psutil_FiletimeToUnixTime(ftLocalTime);
    double bootTime = currentTime - (double)dwTickCount / 1000.0;
    
    return bootTime;
}
