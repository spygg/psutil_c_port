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
#include <iprtrmib.h>
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
    // GetTickCount64 is optional (Windows Vista+)
    g_GetTickCount64 = psutil_GetProcAddress(
        "kernel32", "GetTickCount64");
    // RtlIpv6AddressToStringA is optional (Windows Vista+)
    g_RtlIpv6AddressToStringA = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlIpv6AddressToStringA");

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
    if (maj == 5 && min == 1)
        PSUTIL_WINVER = PSUTIL_WINDOWS_XP;  // Windows XP
    else if (maj == 5 && min == 2)
        PSUTIL_WINVER = PSUTIL_WINDOWS_SERVER_2003;  // Windows Server 2003
    else if (maj == 6 && min == 0)
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

// Process environment block structures for 32-bit processes
typedef struct _PEB32 {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    ULONG_PTR Reserved3[2];
    ULONG_PTR Ldr;
    ULONG_PTR ProcessParameters;
    BYTE Reserved4[104];
    ULONG_PTR Reserved5[52];
    BYTE Reserved6[464];
    ULONG_PTR Reserved7[14];
} PEB32, *PPEB32;

typedef struct _UNICODE_STRING32 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG_PTR Buffer;
} UNICODE_STRING32, *PUNICODE_STRING32;

typedef struct _RTL_USER_PROCESS_PARAMETERS32 {
    BYTE Reserved1[16];
    ULONG_PTR Reserved2[10];
    UNICODE_STRING32 ImagePathName;
    UNICODE_STRING32 CommandLine;
    UNICODE_STRING32 CurrentDirectoryPath;
    ULONG_PTR env;
} RTL_USER_PROCESS_PARAMETERS32, *PRTL_USER_PROCESS_PARAMETERS32;

// Process environment block structures for 64-bit processes
typedef struct _PEB64 {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    ULONG_PTR Reserved3[2];
    ULONG_PTR Ldr;
    ULONG_PTR ProcessParameters;
    BYTE Reserved4[104];
    ULONG_PTR Reserved5[52];
    BYTE Reserved6[464];
    ULONG_PTR Reserved7[14];
} PEB64, *PPEB64;

typedef struct _UNICODE_STRING64 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG_PTR Buffer;
} UNICODE_STRING64, *PUNICODE_STRING64;

typedef struct _RTL_USER_PROCESS_PARAMETERS64 {
    BYTE Reserved1[16];
    ULONG_PTR Reserved2[10];
    UNICODE_STRING64 ImagePathName;
    UNICODE_STRING64 CommandLine;
    UNICODE_STRING64 CurrentDirectoryPath;
    ULONG_PTR env;
} RTL_USER_PROCESS_PARAMETERS64, *PRTL_USER_PROCESS_PARAMETERS64;

char** psutil_windows_process_get_cmdline(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        *count = 0;
        return NULL;
    }

    WCHAR* cmdline = NULL;
    SIZE_T size = 0;

#ifdef _WIN64
    // 64-bit case
    LPVOID ppeb32 = NULL;
    NTSTATUS status = g_NtQuerySystemInformation ? g_NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)53, // SystemProcessIdInformation
        &ppeb32,
        sizeof(LPVOID),
        NULL
    ) : STATUS_UNSUCCESSFUL;

    if (NT_SUCCESS(status) && ppeb32 != NULL) {
        // Target process is 32-bit running in WoW64 mode
        PEB32 peb32;
        RTL_USER_PROCESS_PARAMETERS32 procParameters32;

        if (ReadProcessMemory(hProcess, ppeb32, &peb32, sizeof(peb32), NULL)) {
            if (ReadProcessMemory(
                hProcess,
                (LPVOID)peb32.ProcessParameters,
                &procParameters32,
                sizeof(procParameters32),
                NULL
            )) {
                size = procParameters32.CommandLine.Length;
                if (size > 0) {
                    cmdline = (WCHAR*)malloc(size + 2);
                    if (cmdline) {
                        ReadProcessMemory(
                            hProcess,
                            (LPVOID)procParameters32.CommandLine.Buffer,
                            cmdline,
                            size,
                            NULL
                        );
                        cmdline[size / sizeof(WCHAR)] = L'\0';
                    }
                }
            }
        }
    }
    else {
        // Target process is 64-bit
        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength;
        status = g_NtQueryInformationProcess(
            hProcess,
            ProcessBasicInformation,
            &pbi,
            sizeof(pbi),
            &returnLength
        );

        if (NT_SUCCESS(status)) {
            PEB64 peb64;
            RTL_USER_PROCESS_PARAMETERS64 procParameters64;

            if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb64, sizeof(peb64), NULL)) {
                if (ReadProcessMemory(
                    hProcess,
                    (LPCVOID)peb64.ProcessParameters,
                    &procParameters64,
                    sizeof(procParameters64),
                    NULL
                )) {
                    size = procParameters64.CommandLine.Length;
                    if (size > 0) {
                        cmdline = (WCHAR*)malloc(size + 2);
                        if (cmdline) {
                            ReadProcessMemory(
                                hProcess,
                                (LPCVOID)procParameters64.CommandLine.Buffer,
                                cmdline,
                                size,
                                NULL
                            );
                            cmdline[size / sizeof(WCHAR)] = L'\0';
                        }
                    }
                }
            }
        }
    }
#else
    // 32-bit case
    BOOL weAreWow64, theyAreWow64;
    if (IsWow64Process(GetCurrentProcess(), &weAreWow64) && 
        IsWow64Process(hProcess, &theyAreWow64)) {
        if (weAreWow64 && !theyAreWow64) {
            // We are 32-bit in WoW64, target is 64-bit
            // This is complex, we'll skip for now
        }
        else {
            // Both are 32-bit
            PROCESS_BASIC_INFORMATION pbi;
            ULONG returnLength;
            NTSTATUS status = g_NtQueryInformationProcess(
                hProcess,
                ProcessBasicInformation,
                &pbi,
                sizeof(pbi),
                &returnLength
            );

            if (NT_SUCCESS(status)) {
                PEB32 peb32;
                RTL_USER_PROCESS_PARAMETERS32 procParameters32;

                if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb32, sizeof(peb32), NULL)) {
                    if (ReadProcessMemory(
                        hProcess,
                        (LPVOID)peb32.ProcessParameters,
                        &procParameters32,
                        sizeof(procParameters32),
                        NULL
                    )) {
                        size = procParameters32.CommandLine.Length;
                        if (size > 0) {
                            cmdline = (WCHAR*)malloc(size + 2);
                            if (cmdline) {
                                ReadProcessMemory(
                                    hProcess,
                                    (LPVOID)procParameters32.CommandLine.Buffer,
                                    cmdline,
                                    size,
                                    NULL
                                );
                                cmdline[size / sizeof(WCHAR)] = L'\0';
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    CloseHandle(hProcess);

    if (!cmdline) {
        *count = 0;
        return NULL;
    }

    // Parse command line
    int nArgs;
    LPWSTR* args = CommandLineToArgvW(cmdline, &nArgs);
    if (!args) {
        free(cmdline);
        *count = 0;
        return NULL;
    }

    // Convert to char**
    char** result = (char**)malloc((nArgs + 1) * sizeof(char*));
    if (!result) {
        LocalFree(args);
        free(cmdline);
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < nArgs; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, args[i], -1, NULL, 0, NULL, NULL);
        result[i] = (char*)malloc(len * sizeof(char));
        if (result[i]) {
            WideCharToMultiByte(CP_UTF8, 0, args[i], -1, result[i], len, NULL, NULL);
        }
    }
    result[nArgs] = NULL;

    LocalFree(args);
    free(cmdline);
    *count = nArgs;
    return result;
}

int psutil_windows_process_get_status(Process* proc) {
    if (proc == NULL) {
        return STATUS_DEAD;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return STATUS_DEAD;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return STATUS_DEAD;
    }

    CloseHandle(hProcess);

    if (exitCode == STILL_ACTIVE) {
        // Process is running
        return STATUS_RUNNING;
    } else {
        // Process has exited
        return STATUS_DEAD;
    }
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
    if (proc == NULL) {
        return 0;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return 0;
    }

    DWORD priority = GetPriorityClass(hProcess);
    CloseHandle(hProcess);

    // Map Windows priority classes to nice values
    switch (priority) {
        case IDLE_PRIORITY_CLASS:
            return 19;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return 10;
        case NORMAL_PRIORITY_CLASS:
            return 0;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return -10;
        case HIGH_PRIORITY_CLASS:
            return -15;
        case REALTIME_PRIORITY_CLASS:
            return -20;
        default:
            return 0;
    }
}

int psutil_windows_process_set_nice(Process* proc, int value) {
    if (proc == NULL) {
        return -1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return -1;
    }

    DWORD priority;
    // Map nice values to Windows priority classes
    if (value <= -20) {
        priority = REALTIME_PRIORITY_CLASS;
    } else if (value <= -15) {
        priority = HIGH_PRIORITY_CLASS;
    } else if (value <= -10) {
        priority = ABOVE_NORMAL_PRIORITY_CLASS;
    } else if (value <= 0) {
        priority = NORMAL_PRIORITY_CLASS;
    } else if (value <= 10) {
        priority = BELOW_NORMAL_PRIORITY_CLASS;
    } else {
        priority = IDLE_PRIORITY_CLASS;
    }

    BOOL result = SetPriorityClass(hProcess, priority);
    CloseHandle(hProcess);

    return result ? 0 : -1;
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
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        *count = 0;
        return NULL;
    }

    WCHAR* env = NULL;
    SIZE_T size = 0;

#ifdef _WIN64
    // 64-bit case
    LPVOID ppeb32 = NULL;
    NTSTATUS status = g_NtQuerySystemInformation ? g_NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)53, // SystemProcessIdInformation
        &ppeb32,
        sizeof(LPVOID),
        NULL
    ) : STATUS_UNSUCCESSFUL;

    if (NT_SUCCESS(status) && ppeb32 != NULL) {
        // Target process is 32-bit running in WoW64 mode
        PEB32 peb32;
        RTL_USER_PROCESS_PARAMETERS32 procParameters32;

        if (ReadProcessMemory(hProcess, ppeb32, &peb32, sizeof(peb32), NULL)) {
            if (ReadProcessMemory(
                hProcess,
                (LPVOID)peb32.ProcessParameters,
                &procParameters32,
                sizeof(procParameters32),
                NULL
            )) {
                // Read environment block
                // We'll read a reasonable amount and then find the end
                size = 16384; // 16KB initial buffer
                env = (WCHAR*)malloc(size);
                if (env) {
                    SIZE_T bytesRead;
                    if (ReadProcessMemory(
                        hProcess,
                        (LPVOID)procParameters32.env,
                        env,
                        size,
                        &bytesRead
                    )) {
                        // Find the end of the environment block (double null terminator)
                        for (SIZE_T i = 0; i < bytesRead - 1; i += 2) {
                            if (env[i] == L'\0' && env[i + 2] == L'\0') {
                                size = i + 4;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        // Target process is 64-bit
        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength;
        status = g_NtQueryInformationProcess(
            hProcess,
            ProcessBasicInformation,
            &pbi,
            sizeof(pbi),
            &returnLength
        );

        if (NT_SUCCESS(status)) {
            PEB64 peb64;
            RTL_USER_PROCESS_PARAMETERS64 procParameters64;

            if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb64, sizeof(peb64), NULL)) {
                if (ReadProcessMemory(
                    hProcess,
                    (LPCVOID)peb64.ProcessParameters,
                    &procParameters64,
                    sizeof(procParameters64),
                    NULL
                )) {
                    // Read environment block
                    size = 16384; // 16KB initial buffer
                    env = (WCHAR*)malloc(size);
                    if (env) {
                        SIZE_T bytesRead;
                        if (ReadProcessMemory(
                            hProcess,
                            (LPCVOID)procParameters64.env,
                            env,
                            size,
                            &bytesRead
                        )) {
                            // Find the end of the environment block (double null terminator)
                            for (SIZE_T i = 0; i < bytesRead - 1; i += 2) {
                                if (env[i] == L'\0' && env[i + 2] == L'\0') {
                                    size = i + 4;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#else
    // 32-bit case
    BOOL weAreWow64, theyAreWow64;
    if (IsWow64Process(GetCurrentProcess(), &weAreWow64) && 
        IsWow64Process(hProcess, &theyAreWow64)) {
        if (weAreWow64 && !theyAreWow64) {
            // We are 32-bit in WoW64, target is 64-bit
            // This is complex, we'll skip for now
        }
        else {
            // Both are 32-bit
            PROCESS_BASIC_INFORMATION pbi;
            ULONG returnLength;
            NTSTATUS status = g_NtQueryInformationProcess(
                hProcess,
                ProcessBasicInformation,
                &pbi,
                sizeof(pbi),
                &returnLength
            );

            if (NT_SUCCESS(status)) {
                PEB32 peb32;
                RTL_USER_PROCESS_PARAMETERS32 procParameters32;

                if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb32, sizeof(peb32), NULL)) {
                    if (ReadProcessMemory(
                        hProcess,
                        (LPVOID)peb32.ProcessParameters,
                        &procParameters32,
                        sizeof(procParameters32),
                        NULL
                    )) {
                        // Read environment block
                        size = 16384; // 16KB initial buffer
                        env = (WCHAR*)malloc(size);
                        if (env) {
                            SIZE_T bytesRead;
                            if (ReadProcessMemory(
                                hProcess,
                                (LPVOID)procParameters32.env,
                                env,
                                size,
                                &bytesRead
                            )) {
                                // Find the end of the environment block (double null terminator)
                                for (SIZE_T i = 0; i < bytesRead - 1; i += 2) {
                                    if (env[i] == L'\0' && env[i + 2] == L'\0') {
                                        size = i + 4;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    CloseHandle(hProcess);

    if (!env) {
        *count = 0;
        return NULL;
    }

    // Count environment variables
    int envCount = 0;
    for (SIZE_T i = 0; i < size - 2; i += 2) {
        if (env[i] == L'\0') {
            envCount++;
        }
    }

    // Convert to char**
    char** result = (char**)malloc((envCount + 1) * sizeof(char*));
    if (!result) {
        free(env);
        *count = 0;
        return NULL;
    }

    int index = 0;
    for (SIZE_T i = 0; i < size - 2 && index < envCount; ) {
        if (env[i] != L'\0') {
            int len = WideCharToMultiByte(CP_UTF8, 0, &env[i], -1, NULL, 0, NULL, NULL);
            result[index] = (char*)malloc(len * sizeof(char));
            if (result[index]) {
                WideCharToMultiByte(CP_UTF8, 0, &env[i], -1, result[index], len, NULL, NULL);
            }
            index++;
            // Skip to next variable
            while (i < size - 2 && env[i] != L'\0') {
                i += 2;
            }
        }
        i += 2;
    }
    result[envCount] = NULL;

    free(env);
    *count = envCount;
    return result;
}

int psutil_windows_process_get_num_handles(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return 0;
    }

    DWORD handleCount = 0;
    if (GetProcessHandleCount(hProcess, &handleCount)) {
        CloseHandle(hProcess);
        return (int)handleCount;
    }

    CloseHandle(hProcess);
    return 0;
}

// System process information structure
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    BYTE Reserved1[48];
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    PVOID Reserved2;
    ULONG HandleCount;
    ULONG SessionId;
    PVOID Reserved3;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    PVOID Reserved5;
    SIZE_T QuotaPagedPoolUsage;
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER Reserved7[6];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

psutil_ctx_switches psutil_windows_process_get_num_ctx_switches(Process* proc) {
    psutil_ctx_switches switches = {0};
    if (proc == NULL) {
        return switches;
    }

    if (!g_NtQuerySystemInformation) {
        return switches;
    }

    // Allocate buffer for system process information
    ULONG bufferSize = 1024 * 1024; // 1MB initial buffer
    PVOID buffer = malloc(bufferSize);
    if (!buffer) {
        return switches;
    }

    NTSTATUS status;
    ULONG returnLength;

    // Query system process information
    status = g_NtQuerySystemInformation(
        SystemProcessInformation,
        buffer,
        bufferSize,
        &returnLength
    );

    if (status == STATUS_INFO_LENGTH_MISMATCH) {
        // Buffer too small, reallocate
        free(buffer);
        bufferSize = returnLength;
        buffer = malloc(bufferSize);
        if (!buffer) {
            return switches;
        }

        status = g_NtQuerySystemInformation(
            SystemProcessInformation,
            buffer,
            bufferSize,
            &returnLength
        );
    }

    if (NT_SUCCESS(status)) {
        PSYSTEM_PROCESS_INFORMATION pProcessInfo = (PSYSTEM_PROCESS_INFORMATION)buffer;

        // Iterate through process information to find our process
        while (TRUE) {
            if (pProcessInfo->UniqueProcessId == (HANDLE)(DWORD_PTR)proc->pid) {
                // Found our process, now get thread information
                // Note: SYSTEM_PROCESS_INFORMATION doesn't directly contain context switch counts
                // We'll need to sum context switches from all threads
                // For simplicity, we'll return 0 for now
                break;
            }

            if (pProcessInfo->NextEntryOffset == 0) {
                break; // End of list
            }

            pProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((PBYTE)pProcessInfo + pProcessInfo->NextEntryOffset);
        }
    }

    free(buffer);
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
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        *count = 0;
        return NULL;
    }

    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    // First pass: count threads
    int threadCount = 0;
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == proc->pid) {
                threadCount++;
            }
        } while (Thread32Next(hSnapshot, &te));
    }

    if (threadCount == 0) {
        CloseHandle(hSnapshot);
        *count = 0;
        return NULL;
    }

    // Allocate memory for thread information
    psutil_thread* threads = (psutil_thread*)malloc(threadCount * sizeof(psutil_thread));
    if (!threads) {
        CloseHandle(hSnapshot);
        *count = 0;
        return NULL;
    }

    // Second pass: collect thread information
    int index = 0;
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == proc->pid) {
                threads[index].id = te.th32ThreadID;
                
                // Get thread times
                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                if (hThread) {
                    FILETIME createTime, exitTime, kernelTime, userTime;
                    if (GetThreadTimes(hThread, &createTime, &exitTime, &kernelTime, &userTime)) {
                        threads[index].user_time = psutil_FiletimeToUnixTime(userTime);
                        threads[index].system_time = psutil_FiletimeToUnixTime(kernelTime);
                    }
                    CloseHandle(hThread);
                }
                
                index++;
            }
        } while (Thread32Next(hSnapshot, &te) && index < threadCount);
    }

    CloseHandle(hSnapshot);
    *count = threadCount;
    return threads;
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
        // Convert FILETIME to seconds
        // FILETIME is 100-nanosecond intervals since January 1, 1601
        ULARGE_INTEGER user, kernel;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        
        // Convert to seconds
        times.user = (double)user.QuadPart / 10000000.0;
        times.system = (double)kernel.QuadPart / 10000000.0;
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
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, proc->pid);
    if (hProcess == NULL) {
        *count = 0;
        return NULL;
    }

    // Get system information to determine memory range
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    LPVOID minAddr = si.lpMinimumApplicationAddress;
    LPVOID maxAddr = si.lpMaximumApplicationAddress;

    // Allocate initial buffer for memory maps
    int capacity = 128;
    psutil_memory_map* maps = (psutil_memory_map*)malloc(capacity * sizeof(psutil_memory_map));
    if (!maps) {
        CloseHandle(hProcess);
        *count = 0;
        return NULL;
    }

    int mapCount = 0;
    LPVOID addr = minAddr;

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) == 0) {
            break;
        }

        // Skip reserved memory regions
        if (mbi.State == MEM_RESERVE || mbi.State == MEM_FREE) {
            addr = (LPVOID)((DWORD_PTR)addr + mbi.RegionSize);
            continue;
        }

        // Get mapped file name if available
        char fileName[MAX_PATH] = {0};
        if (mbi.Type == MEM_MAPPED || mbi.Type == MEM_IMAGE) {
            DWORD size = GetMappedFileNameA(hProcess, mbi.BaseAddress, fileName, MAX_PATH);
            if (size == 0) {
                fileName[0] = '\0';
            }
        }

        // Add to memory maps
        if (mapCount >= capacity) {
            capacity *= 2;
            psutil_memory_map* newMaps = (psutil_memory_map*)realloc(maps, capacity * sizeof(psutil_memory_map));
            if (!newMaps) {
                free(maps);
                CloseHandle(hProcess);
                *count = 0;
                return NULL;
            }
            maps = newMaps;
        }

        strncpy(maps[mapCount].path, fileName, sizeof(maps[mapCount].path) - 1);
        maps[mapCount].rss = mbi.RegionSize;
        maps[mapCount].vms = mbi.RegionSize;
        maps[mapCount].shared = 0; // Not easily available on Windows
        maps[mapCount].text = (mbi.Type == MEM_IMAGE) ? mbi.RegionSize : 0;
        maps[mapCount].lib = 0; // Not easily available on Windows
        maps[mapCount].data = (mbi.Type == MEM_PRIVATE) ? mbi.RegionSize : 0;
        maps[mapCount].dirty = 0; // Not easily available on Windows

        mapCount++;
        addr = (LPVOID)((DWORD_PTR)addr + mbi.RegionSize);
    }

    CloseHandle(hProcess);
    *count = mapCount;
    return maps;
}

// Handle information structure
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT UniqueProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR ObjectTypeIndex;
    UCHAR HandleAttributes;
    USHORT HandleValue;
    PVOID Object;
    ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG NumberOfHandles;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

// Object type information structure
typedef struct _OBJECT_TYPE_INFORMATION {
    UNICODE_STRING Name;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccess;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    UCHAR TypeIndex;
    CHAR ReservedByte;
    ULONG PoolType;
    ULONG DefaultPagedPoolCharge;
    ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

psutil_open_file* psutil_windows_process_get_open_files(Process* proc, int* count) {
    if (proc == NULL) {
        *count = 0;
        return NULL;
    }

    if (!g_NtQuerySystemInformation || !g_NtQueryObject) {
        *count = 0;
        return NULL;
    }

    // Allocate buffer for handle information
    ULONG bufferSize = 1024 * 1024; // 1MB initial buffer
    PVOID buffer = malloc(bufferSize);
    if (!buffer) {
        *count = 0;
        return NULL;
    }

    NTSTATUS status;
    ULONG returnLength;

    // Query system handle information
    status = g_NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)16, // SystemHandleInformation
        buffer,
        bufferSize,
        &returnLength
    );

    if (status == STATUS_INFO_LENGTH_MISMATCH) {
        // Buffer too small, reallocate
        free(buffer);
        bufferSize = returnLength;
        buffer = malloc(bufferSize);
        if (!buffer) {
            *count = 0;
            return NULL;
        }

        status = g_NtQuerySystemInformation(
            (SYSTEM_INFORMATION_CLASS)16, // SystemHandleInformation
            buffer,
            bufferSize,
            &returnLength
        );
    }

    if (!NT_SUCCESS(status)) {
        free(buffer);
        *count = 0;
        return NULL;
    }

    PSYSTEM_HANDLE_INFORMATION handleInfo = (PSYSTEM_HANDLE_INFORMATION)buffer;
    int fileHandleCount = 0;

    // First pass: count file handles for this process
    for (ULONG i = 0; i < handleInfo->NumberOfHandles; i++) {
        if (handleInfo->Handles[i].UniqueProcessId == proc->pid) {
            fileHandleCount++;
        }
    }

    if (fileHandleCount == 0) {
        free(buffer);
        *count = 0;
        return NULL;
    }

    // Allocate memory for open files
    psutil_open_file* openFiles = (psutil_open_file*)malloc(fileHandleCount * sizeof(psutil_open_file));
    if (!openFiles) {
        free(buffer);
        *count = 0;
        return NULL;
    }

    // Second pass: collect file information
    int index = 0;
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, proc->pid);
    if (hProcess) {
        for (ULONG i = 0; i < handleInfo->NumberOfHandles && index < fileHandleCount; i++) {
            if (handleInfo->Handles[i].UniqueProcessId == proc->pid) {
                // Duplicate the handle to our process
                HANDLE hDuplicate;
                if (DuplicateHandle(
                    hProcess,
                    (HANDLE)(DWORD_PTR)handleInfo->Handles[i].HandleValue,
                    GetCurrentProcess(),
                    &hDuplicate,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS
                )) {
                    // Get object type information
                    ULONG typeBufferSize = 1024;
                    PVOID typeBuffer = malloc(typeBufferSize);
                    if (typeBuffer) {
                        status = g_NtQueryObject(
                            hDuplicate,
                            ObjectTypeInformation,
                            typeBuffer,
                            typeBufferSize,
                            &returnLength
                        );

                        if (NT_SUCCESS(status)) {
                            POBJECT_TYPE_INFORMATION typeInfo = (POBJECT_TYPE_INFORMATION)typeBuffer;
                            // Check if this is a file handle
                            if (typeInfo->Name.Length > 0 && 
                                wcsncmp(typeInfo->Name.Buffer, L"File", typeInfo->Name.Length / 2) == 0) {
                                // Get file path - use a simple approach for compatibility
                                char filePath[MAX_PATH] = {0};
                                // Note: GetFinalPathNameByHandleA is not available on Windows XP
                                // For compatibility, we'll skip getting the actual file path
                                strncpy(openFiles[index].path, "[File Handle]", sizeof(openFiles[index].path) - 1);
                                openFiles[index].fd = (int)handleInfo->Handles[i].HandleValue;
                                index++;
                            }
                        }
                        free(typeBuffer);
                    }
                    CloseHandle(hDuplicate);
                }
            }
        }
        CloseHandle(hProcess);
    }

    free(buffer);
    *count = index;
    return openFiles;
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
        // Convert FILETIME to seconds
        // FILETIME is 100-nanosecond intervals since January 1, 1601
        ULARGE_INTEGER user, kernel;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        
        // Convert to seconds
        times.user = (double)user.QuadPart / 10000000.0;
        times.system = (double)kernel.QuadPart / 10000000.0 - times.user;
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
    // Note: InetNtopA is not available on Windows XP
    // For compatibility, we'll use a simple format
    snprintf(buf, buf_size, "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%d",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
             addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15],
             ntohs((u_short)port));
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
    
    // Table pointers
    PMIB_TCPTABLE_OWNER_PID tcp_table = NULL;
    PMIB_UDPTABLE_OWNER_PID udp_table = NULL;
    
    if (connections == NULL) {
        return NULL;
    }
    
    // Get TCP IPv4 connections
    if (get_tcp) {
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
    /*
    if (get_tcp6) {
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
    */
    
    // Get UDP IPv4 connections
    if (get_udp) {
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
    /*
    if (get_udp6) {
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
    */
    
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
                
                // Get netmask - use default for compatibility with older Windows versions
                strncpy(addrs[index].netmask, "255.255.255.0", sizeof(addrs[index].netmask) - 1);
                
                // Get broadcast (approximation)
                strncpy(addrs[index].broadcast, "255.255.255.255", sizeof(addrs[index].broadcast) - 1);
            } else if (sockAddr->lpSockaddr->sa_family == AF_INET6) {
                struct sockaddr_in6* sin6 = (struct sockaddr_in6*)sockAddr->lpSockaddr;
                char addrStr[INET6_ADDRSTRLEN];
                // Use wsprintf for compatibility
                wsprintfA(addrStr, "%s", "[IPv6]");
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
    FILETIME ftSystemTime;
    
    // Get system time as FILETIME (UTC)
    GetSystemTimeAsFileTime(&ftSystemTime);
    
    // Get tick count (use GetTickCount64 if available, otherwise GetTickCount)
    uint64_t tickCount = 0;
    if (g_GetTickCount64) {
        tickCount = g_GetTickCount64();
    } else {
        // On Windows XP, GetTickCount returns a 32-bit value that wraps around
        // We'll use it anyway, but note that it's not reliable for systems running longer than 49.7 days
        tickCount = GetTickCount();
    }
    
    // Calculate boot time using UTC time to match time(NULL) which returns UTC
    double currentTime = psutil_FiletimeToUnixTime(ftSystemTime);
    double bootTime = currentTime - (double)tickCount / 1000.0;
    
    return bootTime;
}
