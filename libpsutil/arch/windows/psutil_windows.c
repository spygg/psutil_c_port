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
#include <WS2tcpip.h>
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
PGetCurrentProcessorNumber g_GetCurrentProcessorNumber;
PGetProcessHandleCount g_GetProcessHandleCount;
PGetExtendedTcpTable g_GetExtendedTcpTable;
PGetExtendedUdpTable g_GetExtendedUdpTable;
PGetAdaptersAddresses g_GetAdaptersAddresses;

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

// Converts a wide string to a freshly allocated UTF-8 buffer. Every ANSI API
// used here (GetModuleBaseNameA, LookupAccountSidA, ...) mangles non-ASCII
// names into '?' on Chinese Windows, so all of them are replaced by the W
// variants feeding this helper.
char* psutil_windows_wide_to_utf8(const WCHAR* w, int len) {
    int bytes;
    char* out;

    if (w == NULL) {
        return NULL;
    }
    if (len < 0) {
        len = (int)wcslen(w);
    }
    bytes = WideCharToMultiByte(CP_UTF8, 0, w, len, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        return NULL;
    }
    out = (char*)malloc((size_t)bytes + 1);
    if (out == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, len, out, bytes, NULL, NULL) <= 0) {
        free(out);
        return NULL;
    }
    out[bytes] = '\0';
    return out;
}
PVOID psutil_SetFromNTStatusErr(NTSTATUS Status, const char *syscall) {
    ULONG err;

    err = g_RtlNtStatusToDosErrorNoTeb(Status);
    SetLastError(err);
    return NULL;
}

int psutil_loadlibs(void) {
    // --- Mandatory (available since Windows XP)
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
    g_RtlGetVersion = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlGetVersion");
    if (!g_RtlGetVersion)
        return 1;

    // --- Optional (Vista+ / Win7+). Missing entries stay NULL and
    //     callers must check before use, so XP keeps working.
    // Vista+
    g_RtlIpv4AddressToStringA = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlIpv4AddressToStringA");
    g_RtlIpv6AddressToStringA = psutil_GetProcAddressFromLib(
        "ntdll.dll", "RtlIpv6AddressToStringA");
    g_NtSuspendProcess = psutil_GetProcAddressFromLib(
        "ntdll", "NtSuspendProcess");
    g_NtResumeProcess = psutil_GetProcAddressFromLib(
        "ntdll", "NtResumeProcess");
    g_NtQueryVirtualMemory = psutil_GetProcAddressFromLib(
        "ntdll", "NtQueryVirtualMemory");
    g_RtlNtStatusToDosErrorNoTeb = psutil_GetProcAddressFromLib(
        "ntdll", "RtlNtStatusToDosErrorNoTeb");
    g_GetTickCount64 = psutil_GetProcAddress(
        "kernel32", "GetTickCount64");
    // Vista+
    g_GetCurrentProcessorNumber = psutil_GetProcAddress(
        "kernel32", "GetCurrentProcessorNumber");
    g_GetProcessHandleCount = psutil_GetProcAddress(
        "kernel32", "GetProcessHandleCount");
    // IPHLPAPI functions (need XP SP2+; load dynamically for XP RTM/SP1)
    g_GetExtendedTcpTable = psutil_GetProcAddressFromLib(
        "iphlpapi.dll", "GetExtendedTcpTable");
    g_GetExtendedUdpTable = psutil_GetProcAddressFromLib(
        "iphlpapi.dll", "GetExtendedUdpTable");
    g_GetAdaptersAddresses = psutil_GetProcAddressFromLib(
        "iphlpapi.dll", "GetAdaptersAddresses");
    // Win7+
    g_GetActiveProcessorCount = psutil_GetProcAddress(
        "kernel32", "GetActiveProcessorCount");
    g_GetLogicalProcessorInformationEx = psutil_GetProcAddressFromLib(
        "kernel32", "GetLogicalProcessorInformationEx");
    // WTS (Windows Server Core etc.)
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
int psutil_windows_pid_exists(psutil_pid_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (hProcess != NULL) {
        CloseHandle(hProcess);
        return 1;
    }
    if (GetLastError() == ERROR_ACCESS_DENIED) {
        return 1;
    }
    return 0;
}

psutil_pid_t* psutil_windows_pids(int *count) {
    DWORD *pids = NULL;
    DWORD size = 1024;
    DWORD needed;

    pids = (DWORD*)malloc(size * sizeof(DWORD));
    if (pids == NULL) {
        *count = 0;
        return NULL;
    }

    // EnumProcesses only reports the required size through `needed` when the
    // buffer was too small. The old code gave up in exactly that case, so on
    // any machine with more than ~1024 processes pids() returned NULL.
    for (;;) {
        DWORD* np = (DWORD*)realloc(pids, (size_t)size * sizeof(DWORD));
        if (np == NULL) {
            free(pids);
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
        pids = np;
        if (!EnumProcesses(pids, size * sizeof(DWORD), &needed)) {
            free(pids);
            psutil_set_last_error(PSUTIL_ERROR_IO);
            return NULL;
        }
        if (needed < size * sizeof(DWORD)) {
            break;
        }
        size = needed / sizeof(DWORD) + 16;
    }

    *count = (int)(needed / sizeof(DWORD));
    return (psutil_pid_t*)pids;
}

Process* psutil_windows_process_new(psutil_pid_t pid) {
    Process* proc = (Process*)malloc(sizeof(Process));
    if (proc == NULL) {
        return NULL;
    }

    // If pid is 0, use current process PID
    if (pid == 0) {
        pid = (psutil_pid_t)GetCurrentProcessId();
    }

    proc->pid = pid;
    proc->name = NULL;
    proc->exe = NULL;
    proc->cwd = NULL;
    proc->username = NULL;
    proc->create_time = 0.0;
    proc->ppid = 0;
    proc->status = PSUTIL_STATUS_RUNNING;
    proc->_platform = NULL;

    // Getters are lazy: fetching name/exe/username/cwd/create_time here made
    // every single process_new() call open the process up to five times (and
    // run a potentially network-bound LookupAccountSid). process_refresh()
    // and the individual getters fill these in on demand.
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

psutil_pid_t psutil_windows_process_get_ppid(Process* proc) {
    if (proc == NULL) {
        return 0;
    }

    if (proc->ppid > 0) {
        return proc->ppid;
    }

    if (g_NtQueryInformationProcess == NULL) {
        return 0;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return 0;
    }

    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength = 0;
    NTSTATUS status = g_NtQueryInformationProcess(
        hProcess,
        ProcessBasicInformation,
        &pbi,
        sizeof(pbi),
        &returnLength
    );

    CloseHandle(hProcess);

    if (NT_SUCCESS(status) && returnLength >= sizeof(pbi)) {
        proc->ppid = (DWORD)(DWORD_PTR)pbi.InheritedFromUniqueProcessId;
    }

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
    
    {
        WCHAR name[MAX_PATH];
        DWORD size = MAX_PATH;
        if (GetModuleBaseNameW(hProcess, NULL, name, size) > 0) {
            proc->name = psutil_windows_wide_to_utf8(name, -1);
        } else {
            psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        }
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
    
    {
        WCHAR exe[MAX_PATH];
        DWORD size = MAX_PATH;
        if (GetModuleFileNameExW(hProcess, NULL, exe, size) > 0) {
            proc->exe = psutil_windows_wide_to_utf8(exe, -1);
        } else {
            psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        }
    }
    
    CloseHandle(hProcess);
    return proc->exe;
}

// --- Process environment block layouts -----------------------------------
// These structs are built by the *host* binary, so the 32-bit variants must
// use 32-bit-width fields even on x64: ULONG_PTR is 8 bytes there, which
// silently shifted every following offset and made WOW64 PEB reads garbage.
typedef struct _UNICODE_STRING32 {
    USHORT Length;
    USHORT MaximumLength;
    DWORD Buffer;
} UNICODE_STRING32;

typedef struct _UNICODE_STRING64 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG Reserved;
    ULONG_PTR Buffer;
} UNICODE_STRING64;

// PEB layout: BeingDebugged at 0x02, Ldr at 0x0C (x86) / 0x18 (x64),
// ProcessParameters at 0x10 (x86) / 0x20 (x64).
typedef struct _PEB32 {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    DWORD Reserved3[2];
    DWORD Ldr;
    DWORD ProcessParameters;
} PEB32;

typedef struct _PEB64 {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    ULONG_PTR Reserved3[2];
    ULONG_PTR Ldr;
    ULONG_PTR ProcessParameters;
} PEB64;

// CommandLine lives at a fixed offset inside RTL_USER_PROCESS_PARAMETERS.
// Reading it by offset is deliberate: the full structure is large and version
// dependent (it also starts with a CURDIR block that the old definition
// forgot), while these two offsets are stable.
#define PSUTIL_PARAMS_CMDLINE_OFFSET32 0x40
#define PSUTIL_PARAMS_CMDLINE_OFFSET64 0x80

static WCHAR* psutil_windows_read_cmdline_w(HANDLE hProcess, ULONG_PTR params,
                                            int is32) {
    ULONG_PTR buffer = 0;
    USHORT len = 0;
    WCHAR* out;

    if (params == 0) {
        return NULL;
    }

    if (is32) {
        UNICODE_STRING32 us;
        ULONG_PTR addr = (ULONG_PTR)(DWORD)params + PSUTIL_PARAMS_CMDLINE_OFFSET32;
        if (!ReadProcessMemory(hProcess, (LPCVOID)addr, &us, sizeof(us), NULL)) {
            return NULL;
        }
        len = us.Length;
        buffer = (ULONG_PTR)us.Buffer;
    } else {
        UNICODE_STRING64 us;
        ULONG_PTR addr = params + PSUTIL_PARAMS_CMDLINE_OFFSET64;
        if (!ReadProcessMemory(hProcess, (LPCVOID)addr, &us, sizeof(us), NULL)) {
            return NULL;
        }
        len = us.Length;
        buffer = us.Buffer;
    }

    if (len == 0 || len > 64 * 1024 || buffer == 0) {
        return NULL;
    }
    out = (WCHAR*)malloc((size_t)len + sizeof(WCHAR));
    if (out == NULL) {
        return NULL;
    }
    if (!ReadProcessMemory(hProcess, (LPCVOID)buffer, out, len, NULL)) {
        free(out);
        return NULL;
    }
    out[len / sizeof(WCHAR)] = L'\0';
    return out;
}

char** psutil_windows_process_get_cmdline(Process* proc, int* count) {
    HANDLE hProcess;
    WCHAR* cmdline = NULL;
    LPWSTR* args;
    char** result;
    int nArgs = 0;
    int i;

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
                           proc->pid);
    if (hProcess == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }

#ifdef _WIN64
    {
        LPVOID ppeb32 = NULL;
        ULONG wow_len = 0;
        BOOL is_wow64 = FALSE;

        if (g_NtQueryInformationProcess != NULL) {
            is_wow64 = NT_SUCCESS(g_NtQueryInformationProcess(
                hProcess, (PROCESSINFOCLASS)26, &ppeb32, sizeof(ppeb32),
                &wow_len)) && ppeb32 != NULL;
        }

        if (is_wow64) {
            PEB32 peb32;
            if (ReadProcessMemory(hProcess, ppeb32, &peb32, sizeof(peb32), NULL)) {
                cmdline = psutil_windows_read_cmdline_w(
                    hProcess, (ULONG_PTR)peb32.ProcessParameters, 1);
            }
        } else {
            PROCESS_BASIC_INFORMATION pbi;
            ULONG ret_len = 0;
            if (g_NtQueryInformationProcess != NULL &&
                NT_SUCCESS(g_NtQueryInformationProcess(
                    hProcess, ProcessBasicInformation, &pbi, sizeof(pbi),
                    &ret_len))) {
                PEB64 peb64;
                if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb64,
                                      sizeof(peb64), NULL)) {
                    cmdline = psutil_windows_read_cmdline_w(
                        hProcess, peb64.ProcessParameters, 0);
                }
            }
        }
    }
#else
    {
        PROCESS_BASIC_INFORMATION pbi;
        ULONG ret_len = 0;
        if (g_NtQueryInformationProcess != NULL &&
            NT_SUCCESS(g_NtQueryInformationProcess(
                hProcess, ProcessBasicInformation, &pbi, sizeof(pbi),
                &ret_len))) {
            PEB32 peb32;
            if (ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb32,
                                  sizeof(peb32), NULL)) {
                cmdline = psutil_windows_read_cmdline_w(
                    hProcess, (ULONG_PTR)peb32.ProcessParameters, 1);
            }
        }
    }
#endif

    CloseHandle(hProcess);

    if (cmdline == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }

    args = CommandLineToArgvW(cmdline, &nArgs);
    free(cmdline);
    if (args == NULL || nArgs <= 0) {
        if (args != NULL) {
            LocalFree(args);
        }
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return NULL;
    }

    result = (char**)calloc((size_t)nArgs + 1, sizeof(char*));
    if (result == NULL) {
        LocalFree(args);
        psutil_set_last_error(PSUTIL_ERROR_MEMORY);
        return NULL;
    }
    for (i = 0; i < nArgs; i++) {
        result[i] = psutil_windows_wide_to_utf8(args[i], -1);
        if (result[i] == NULL) {
            int j;
            for (j = 0; j < i; j++) {
                free(result[j]);
            }
            free(result);
            LocalFree(args);
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
    }
    result[nArgs] = NULL;

    LocalFree(args);
    *count = nArgs;
    return result;
}

int psutil_windows_process_get_status(Process* proc) {
    if (proc == NULL) {
        return PSUTIL_STATUS_DEAD;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return PSUTIL_STATUS_DEAD;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return PSUTIL_STATUS_DEAD;
    }

    CloseHandle(hProcess);

    if (exitCode == STILL_ACTIVE) {
        // Process is running
        return PSUTIL_STATUS_RUNNING;
    } else {
        // Process has exited
        return PSUTIL_STATUS_DEAD;
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
                WCHAR username[256];
                WCHAR domain[256];
                DWORD usernameSize = 256;
                DWORD domainSize = 256;
                SID_NAME_USE sidType;

                if (LookupAccountSidW(NULL, pTokenUser->User.Sid, username,
                                      &usernameSize, domain, &domainSize,
                                      &sidType)) {
                    char* user_utf8 = psutil_windows_wide_to_utf8(username, -1);
                    char* domain_utf8 = psutil_windows_wide_to_utf8(domain, -1);
                    if (user_utf8 != NULL) {
                        size_t need = strlen(user_utf8) + 1;
                        char* full;
                        if (domain_utf8 != NULL && domain_utf8[0] != '\0') {
                            need += strlen(domain_utf8) + 1;
                        }
                        full = (char*)malloc(need);
                        if (full != NULL) {
                            if (domain_utf8 != NULL && domain_utf8[0] != '\0') {
                                snprintf(full, need, "%s\\%s", domain_utf8, user_utf8);
                            } else {
                                snprintf(full, need, "%s", user_utf8);
                            }
                            proc->username = full;
                        }
                        free(user_utf8);
                    }
                    free(domain_utf8);
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
    
    // Getting CWD for another process on Windows requires reading the PEB.
    // For the current process, use GetCurrentDirectoryA.
    // For other processes, this is complex and requires PEB traversal.
    if (proc->pid == GetCurrentProcessId()) {
        WCHAR cwd[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, cwd) > 0) {
            proc->cwd = psutil_windows_wide_to_utf8(cwd, -1);
        }
        return proc->cwd;
    }

    // For other processes, try via PEB (Windows Vista+)
    // This is limited and may fail without sufficient privileges.
    // Return NULL if we cannot determine it.
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
    if (proc == NULL) {
        return uids;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return uids;
    }

    HANDLE hToken;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD size = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &size);
        if (size > 0) {
            PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(size);
            if (pTokenUser != NULL) {
                if (GetTokenInformation(hToken, TokenUser, pTokenUser, size, &size)) {
                    uids.real = (uint32_t)(DWORD_PTR)pTokenUser->User.Sid;
                    uids.effective = uids.real;
                    uids.saved = uids.real;
                }
                free(pTokenUser);
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);
    return uids;
}

psutil_gids psutil_windows_process_get_gids(Process* proc) {
    psutil_gids gids = {0};
    if (proc == NULL) {
        return gids;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return gids;
    }

    HANDLE hToken;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD size = 0;
        GetTokenInformation(hToken, TokenPrimaryGroup, NULL, 0, &size);
        if (size > 0) {
            PTOKEN_PRIMARY_GROUP pPrimaryGroup = (PTOKEN_PRIMARY_GROUP)malloc(size);
            if (pPrimaryGroup != NULL) {
                if (GetTokenInformation(hToken, TokenPrimaryGroup, pPrimaryGroup, size, &size)) {
                    gids.real = (uint32_t)(DWORD_PTR)pPrimaryGroup->PrimaryGroup;
                    gids.effective = gids.real;
                    gids.saved = gids.real;
                }
                free(pPrimaryGroup);
            }
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);
    return gids;
}

const char* psutil_windows_process_get_terminal(Process* proc) {
    // Windows does not have a Unix-style terminal concept for processes.
    // Return NULL as terminals are not applicable on Windows.
    (void)proc;
    return NULL;
}

int psutil_windows_process_get_num_fds(Process* proc) {
    // On Windows, there are no file descriptors in the Unix sense.
    // Return the handle count as the closest equivalent.
    return psutil_windows_process_get_num_handles(proc);
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
    // Windows does not have a direct I/O priority concept like Linux ionice.
    // We return 0 (normal) as a default.
    (void)proc;
    return 0;
}

int psutil_windows_process_set_ionice(Process* proc, int ioclass, int value) {
    // Windows does not support setting I/O priority like Linux ionice.
    // Return -1 to indicate not supported.
    (void)proc; (void)ioclass; (void)value;
    return -1;
}

int* psutil_windows_process_get_cpu_affinity(Process* proc, int* count) {
    *count = 0;
    if (proc == NULL) {
        return NULL;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return NULL;
    }

    DWORD_PTR processAffinityMask = 0;
    DWORD_PTR systemAffinityMask = 0;
    int* cpus = NULL;

    if (GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask)) {
        int cpu_count = 0;
        for (int i = 0; i < (int)(sizeof(DWORD_PTR) * 8); i++) {
            if (processAffinityMask & ((DWORD_PTR)1 << i)) {
                cpu_count++;
            }
        }
        if (cpu_count > 0) {
            cpus = (int*)malloc(cpu_count * sizeof(int));
            if (cpus != NULL) {
                int index = 0;
                for (int i = 0; i < (int)(sizeof(DWORD_PTR) * 8); i++) {
                    if (processAffinityMask & ((DWORD_PTR)1 << i)) {
                        cpus[index++] = i;
                    }
                }
                *count = cpu_count;
            }
        }
    }

    CloseHandle(hProcess);
    return cpus;
}

int psutil_windows_process_set_cpu_affinity(Process* proc, int* cpus, int count) {
    if (proc == NULL || cpus == NULL || count <= 0) {
        return -1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc->pid);
    if (hProcess == NULL) {
        return -1;
    }

    DWORD_PTR mask = 0;
    for (int i = 0; i < count; i++) {
        if (cpus[i] >= 0 && cpus[i] < (int)(sizeof(DWORD_PTR) * 8)) {
            mask |= ((DWORD_PTR)1 << cpus[i]);
        }
    }

    BOOL result = SetProcessAffinityMask(hProcess, mask);
    CloseHandle(hProcess);
    return result ? 0 : -1;
}

int psutil_windows_process_get_cpu_num(Process* proc) {
    if (proc == NULL) {
        return -1;
    }
    // On Windows, there is no direct API to get the current CPU for a process.
    // GetCurrentProcessorNumber() is available on Windows Vista+ only.
    // Load it dynamically so the binary can still run on Windows XP.
    if (proc->pid == GetCurrentProcessId() && g_GetCurrentProcessorNumber != NULL) {
        return (int)g_GetCurrentProcessorNumber();
    }
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
    // First try to detect whether the target process is a WoW64 (32-bit) process
    // running under WOW64. ProcessWow64Information (class 26) returns the
    // WoW64 PEB pointer when the process is 32-bit, otherwise NULL.
    LPVOID ppeb32 = NULL;
    ULONG wow64Len = 0;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if (g_NtQueryInformationProcess) {
        status = g_NtQueryInformationProcess(
            hProcess,
            (PROCESSINFOCLASS)26, // ProcessWow64Information
            &ppeb32,
            sizeof(ppeb32),
            &wow64Len
        );
        if (!NT_SUCCESS(status)) {
            ppeb32 = NULL;
        }
    }

    if (ppeb32 != NULL) {
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
                        for (SIZE_T i = 0; i + 1 < bytesRead; i++) {
                            if (env[i] == L'\0' && env[i + 1] == L'\0') {
                                size = (i + 2) * sizeof(WCHAR);
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
                            for (SIZE_T i = 0; i + 1 < bytesRead; i++) {
                                if (env[i] == L'\0' && env[i + 1] == L'\0') {
                                    size = (i + 2) * sizeof(WCHAR);
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
                                for (SIZE_T i = 0; i + 1 < bytesRead; i++) {
                                    if (env[i] == L'\0' && env[i + 1] == L'\0') {
                                        size = (i + 2) * sizeof(WCHAR);
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

    // Count environment variables (WCHAR array of NUL-terminated strings)
    int envCount = 0;
    for (SIZE_T i = 0; i + 1 < size / sizeof(WCHAR); ) {
        if (env[i] == L'\0') {
            envCount++;
            i++;
        } else {
            while (i < size / sizeof(WCHAR) && env[i] != L'\0') {
                i++;
            }
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
    SIZE_T total = size / sizeof(WCHAR);
    for (SIZE_T i = 0; i < total && index < envCount; ) {
        if (env[i] != L'\0') {
            int len = WideCharToMultiByte(CP_UTF8, 0, &env[i], -1, NULL, 0, NULL, NULL);
            result[index] = (char*)malloc((size_t)len * sizeof(char));
            if (result[index]) {
                WideCharToMultiByte(CP_UTF8, 0, &env[i], -1, result[index], len, NULL, NULL);
            }
            index++;
            // Skip to next variable
            while (i < total && env[i] != L'\0') {
                i++;
            }
        }
        i++;
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
    if (g_GetProcessHandleCount != NULL && g_GetProcessHandleCount(hProcess, &handleCount)) {
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
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return 0;
    }
    {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == proc->pid) {
                    num_threads++;
                }
            } while (Thread32Next(hSnapshot, &te));
        }
    }
    CloseHandle(hSnapshot);

    // Returning 1 when nothing was found (or when the snapshot failed) hid
    // "process is gone / access denied" behind a plausible-looking number.
    return num_threads;
}

psutil_thread* psutil_windows_process_get_threads(Process* proc, int* count) {
    HANDLE hSnapshot;
    THREADENTRY32 te;
    psutil_thread* threads = NULL;
    int capacity = 0;
    int index = 0;

    *count = 0;
    if (proc == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
        return NULL;
    }

    // Single pass with a growing array: the old code enumerated the snapshot
    // twice and could report more threads than it actually filled in.
    te.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(hSnapshot, &te)) {
        do {
            HANDLE hThread;
            FILETIME create_time, exit_time, kernel_time, user_time;

            if (te.th32OwnerProcessID != proc->pid) {
                continue;
            }

            if (index >= capacity) {
                int ncap = (capacity == 0) ? 32 : (capacity * 2);
                psutil_thread* nt = (psutil_thread*)realloc(
                    threads, (size_t)ncap * sizeof(psutil_thread));
                if (nt == NULL) {
                    free(threads);
                    CloseHandle(hSnapshot);
                    psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                    return NULL;
                }
                threads = nt;
                capacity = ncap;
            }

            threads[index].id = (uint32_t)te.th32ThreadID;
            threads[index].user_time = 0.0;
            threads[index].system_time = 0.0;

            hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                                 te.th32ThreadID);
            if (hThread != NULL) {
                if (GetThreadTimes(hThread, &create_time, &exit_time,
                                   &kernel_time, &user_time)) {
                    ULARGE_INTEGER u;
                    ULARGE_INTEGER k;
                    u.LowPart = user_time.dwLowDateTime;
                    u.HighPart = user_time.dwHighDateTime;
                    k.LowPart = kernel_time.dwLowDateTime;
                    k.HighPart = kernel_time.dwHighDateTime;
                    // FILETIME is 100ns ticks since 1601; the API exposes
                    // seconds of CPU time, so this is a duration, not a
                    // timestamp: dividing by 10^7 is correct, passing the raw
                    // FILETIME to FiletimeToUnixTime (as before) was not.
                    threads[index].user_time = (double)u.QuadPart / 10000000.0;
                    threads[index].system_time = (double)k.QuadPart / 10000000.0;
                }
                CloseHandle(hThread);
            }
            index++;
        } while (Thread32Next(hSnapshot, &te));
    }

    CloseHandle(hSnapshot);
    *count = index;
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
    // On Windows, we use the same GetProcessMemoryInfo as basic memory info.
    // USS/PSS are Linux concepts and not directly available on Windows.
    return psutil_windows_process_get_memory_info(proc);
}

double psutil_windows_process_get_memory_percent(Process* proc, const char* memtype) {
    if (proc == NULL) {
        return 0.0;
    }

    psutil_memory_info proc_mem = psutil_windows_process_get_memory_info(proc);
    psutil_memory_info sys_mem = psutil_windows_virtual_memory();

    // Use rss by default, or vms if memtype is "vms"
    uint64_t proc_value = proc_mem.rss;
    if (memtype != NULL && strcmp(memtype, "vms") == 0) {
        proc_value = proc_mem.vms;
    }

    if (sys_mem.vms == 0) {
        return 0.0;
    }
    return (double)proc_value / (double)sys_mem.vms * 100.0;
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

psutil_net_connection* psutil_windows_net_connections(const char* kind, int* count) {
    return psutil_windows_net_connections_filtered(kind, 0, count);
}

psutil_net_connection* psutil_windows_process_get_net_connections(Process* proc,
                                                                  const char* kind,
                                                                  int* count) {
    if (proc == NULL) {
        *count = 0;
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }
    // Rows come from TCP_TABLE_OWNER_PID_ALL / UDP_TABLE_OWNER_PID, so every
    // entry carries dwOwningPid and can be matched to this process. The old
    // stub simply returned the whole system table.
    return psutil_windows_net_connections_filtered(kind, proc->pid, count);
}

int psutil_windows_process_send_signal(Process* proc, int sig) {
    if (proc == NULL) {
        return -1;
    }
    // Windows does not support Unix signals. We map common signals to Win32 equivalents.
    switch (sig) {
        case 15:  // SIGTERM
            return psutil_windows_process_terminate(proc);
        case 9:   // SIGKILL
            return psutil_windows_process_kill(proc);
        case 18:  // SIGCONT
            return psutil_windows_process_resume(proc);
        case 19:  // SIGSTOP
            return psutil_windows_process_suspend(proc);
        default:
            // Unsupported signal
            return -1;
    }
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
        info.available = memInfo.ullAvailPhys;
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
        info.available = memInfo.ullAvailPageFile;
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
        ULARGE_INTEGER user, kernel, idle;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;

        times.user = (double)user.QuadPart / 10000000.0;
        // GetSystemTimes' kernel time already contains the idle time.
        times.system = (double)(kernel.QuadPart - idle.QuadPart) / 10000000.0;
        times.idle = (double)idle.QuadPart / 10000000.0;
    }
    return times;
}

double psutil_windows_cpu_percent(double interval, int percpu) {
    // percpu is not supported with a single double return value.
    // We calculate overall CPU usage.
    (void)percpu;
    if (interval <= 0) {
        return 0.0;
    }

    // Get initial CPU times
    FILETIME idleTime1, kernelTime1, userTime1;
    FILETIME idleTime2, kernelTime2, userTime2;

    if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) {
        return 0.0;
    }

    // Sleep for the specified interval (use Sleep, not usleep)
    DWORD sleepMs = (DWORD)(interval * 1000.0);
    Sleep(sleepMs);

    if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) {
        return 0.0;
    }

    // Calculate differences
    ULARGE_INTEGER idle1, kernel1, user1, idle2, kernel2, user2;
    idle1.LowPart = idleTime1.dwLowDateTime; idle1.HighPart = idleTime1.dwHighDateTime;
    kernel1.LowPart = kernelTime1.dwLowDateTime; kernel1.HighPart = kernelTime1.dwHighDateTime;
    user1.LowPart = userTime1.dwLowDateTime; user1.HighPart = userTime1.dwHighDateTime;
    idle2.LowPart = idleTime2.dwLowDateTime; idle2.HighPart = idleTime2.dwHighDateTime;
    kernel2.LowPart = kernelTime2.dwLowDateTime; kernel2.HighPart = kernelTime2.dwHighDateTime;
    user2.LowPart = userTime2.dwLowDateTime; user2.HighPart = userTime2.dwHighDateTime;

    double userDiff = (double)(user2.QuadPart - user1.QuadPart) / 10000000.0;
    double kernelDiff = (double)(kernel2.QuadPart - kernel1.QuadPart) / 10000000.0;
    double idleDiff = (double)(idle2.QuadPart - idle1.QuadPart) / 10000000.0;

    double total = userDiff + kernelDiff;
    double busy = total - idleDiff;

    if (total <= 0) {
        return 0.0;
    }

    return (busy / total) * 100.0;
}

psutil_cpu_times psutil_windows_cpu_times_percent(double interval, int percpu) {
    psutil_cpu_times times = {0};
    (void)percpu;
    if (interval <= 0) {
        return times;
    }

    FILETIME idleTime1, kernelTime1, userTime1;
    FILETIME idleTime2, kernelTime2, userTime2;

    if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) {
        return times;
    }

    Sleep((DWORD)(interval * 1000.0));

    if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) {
        return times;
    }

    ULARGE_INTEGER idle1, kernel1, user1, idle2, kernel2, user2;
    idle1.LowPart = idleTime1.dwLowDateTime; idle1.HighPart = idleTime1.dwHighDateTime;
    kernel1.LowPart = kernelTime1.dwLowDateTime; kernel1.HighPart = kernelTime1.dwHighDateTime;
    user1.LowPart = userTime1.dwLowDateTime; user1.HighPart = userTime1.dwHighDateTime;
    idle2.LowPart = idleTime2.dwLowDateTime; idle2.HighPart = idleTime2.dwHighDateTime;
    kernel2.LowPart = kernelTime2.dwLowDateTime; kernel2.HighPart = kernelTime2.dwHighDateTime;
    user2.LowPart = userTime2.dwLowDateTime; user2.HighPart = userTime2.dwHighDateTime;

    double userDiff = (double)(user2.QuadPart - user1.QuadPart) / 10000000.0;
    double kernelDiff = (double)(kernel2.QuadPart - kernel1.QuadPart) / 10000000.0;
    double idleDiff = (double)(idle2.QuadPart - idle1.QuadPart) / 10000000.0;
    double total = userDiff + kernelDiff;

    if (total > 0) {
        times.user = (userDiff / total) * 100.0;
        times.system = ((kernelDiff - idleDiff) / total) * 100.0;
        times.idle = (idleDiff / total) * 100.0;
    }

    return times;
}

int psutil_windows_cpu_count(int logical) {
    if (logical) {
        return PSUTIL_SYSTEM_INFO.dwNumberOfProcessors;
    }
    // Physical core count using GetLogicalProcessorInformationEx
    if (g_GetLogicalProcessorInformationEx != NULL) {
        DWORD size = 0;
        g_GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &size);
        if (size > 0) {
            UCHAR* buffer = (UCHAR*)malloc(size);
            if (buffer != NULL) {
                if (g_GetLogicalProcessorInformationEx(RelationProcessorCore, buffer, &size)) {
                    ULONG count = 0;
                    UCHAR* ptr = buffer;
                    UCHAR* end = buffer + size;
                    while (ptr < end) {
                        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
                        if (info->Relationship == RelationProcessorCore) {
                            count++;
                        }
                        ptr += info->Size;
                    }
                    free(buffer);
                    return (int)count;
                }
                free(buffer);
            }
        }
    }
    return PSUTIL_SYSTEM_INFO.dwNumberOfProcessors;
}

psutil_cpu_stats psutil_windows_cpu_stats(void) {
    psutil_cpu_stats stats = {0};
    // Windows does not expose context switches, interrupts, soft interrupts,
    // and syscalls counts in a straightforward manner via public APIs.
    // On Windows XP/Vista/7, we could use NtQuerySystemInformation with
    // SystemPerformanceInformation (class 2) which returns a structure
    // containing ContextSwitches and other counters. However, the structure
    // layout varies between Windows versions and is undocumented.
    // We return zeros for fields we cannot reliably fill.
    // This is consistent with the original Python psutil approach on Windows
    // where some of these values are not available.
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
        case MIB_TCP_STATE_CLOSED:     return PSUTIL_CONN_CLOSE;
        case MIB_TCP_STATE_LISTEN:     return PSUTIL_CONN_LISTEN;
        case MIB_TCP_STATE_SYN_SENT:   return PSUTIL_CONN_SYN_SENT;
        case MIB_TCP_STATE_SYN_RCVD:   return PSUTIL_CONN_SYN_RECV;
        case MIB_TCP_STATE_ESTAB:      return PSUTIL_CONN_ESTABLISHED;
        case MIB_TCP_STATE_FIN_WAIT1:  return PSUTIL_CONN_FIN_WAIT1;
        case MIB_TCP_STATE_FIN_WAIT2:  return PSUTIL_CONN_FIN_WAIT2;
        case MIB_TCP_STATE_CLOSE_WAIT: return PSUTIL_CONN_CLOSE_WAIT;
        case MIB_TCP_STATE_CLOSING:    return PSUTIL_CONN_CLOSING;
        case MIB_TCP_STATE_LAST_ACK:   return PSUTIL_CONN_LAST_ACK;
        case MIB_TCP_STATE_TIME_WAIT:  return PSUTIL_CONN_TIME_WAIT;
        default:                       return PSUTIL_CONN_NONE;
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

static psutil_net_connection* psutil_windows_net_connections_filtered(const char* kind, psutil_pid_t pid_filter, int* count) {
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
        
        if (g_GetExtendedTcpTable == NULL) {
            // GetExtendedTcpTable requires XP SP2+; degrade gracefully
            get_tcp = 0;
        } else {
        // Get required size
        g_GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        tcp_table = (PMIB_TCPTABLE_OWNER_PID)malloc(size);
        
        if (tcp_table != NULL && g_GetExtendedTcpTable(tcp_table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
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
                

                if (pid_filter != 0 && tcp_table->table[i].dwOwningPid != (DWORD)pid_filter) {
                    continue;
                }               connections[index].fd = -1;
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
    }
    
    // Get TCP IPv6 connections
    if (get_tcp6) {
        DWORD size = 0;
        PMIB_TCP6TABLE_OWNER_PID tcp6_table = NULL;

        if (g_GetExtendedTcpTable == NULL) {
            get_tcp6 = 0;
        } else {
        g_GetExtendedTcpTable(NULL, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
        if (size > 0) {
            tcp6_table = (PMIB_TCP6TABLE_OWNER_PID)malloc(size);
        }
        
        if (tcp6_table != NULL && g_GetExtendedTcpTable(tcp6_table, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
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
                

                if (pid_filter != 0 && tcp6_table->table[i].dwOwningPid != (DWORD)pid_filter) {
                    continue;
                }               connections[index].fd = -1;
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
    }
    
    // Get UDP IPv4 connections
    if (get_udp) {
        DWORD size = 0;
        
        if (g_GetExtendedUdpTable == NULL) {
            get_udp = 0;
        } else {
        g_GetExtendedUdpTable(NULL, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        udp_table = (PMIB_UDPTABLE_OWNER_PID)malloc(size);
        
        if (udp_table != NULL && g_GetExtendedUdpTable(udp_table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
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
                

                if (pid_filter != 0 && udp_table->table[i].dwOwningPid != (DWORD)pid_filter) {
                    continue;
                }               connections[index].fd = -1;
                connections[index].family = AF_INET;
                connections[index].type = SOCK_DGRAM;
                format_address(connections[index].laddr, sizeof(connections[index].laddr), 
                              udp_table->table[i].dwLocalAddr, udp_table->table[i].dwLocalPort);
                connections[index].raddr[0] = '\0';
                connections[index].status = PSUTIL_CONN_NONE;
                index++;
            }
        }
        free(udp_table);
        }
    }
    
    // Get UDP IPv6 connections
    if (get_udp6) {
        DWORD size = 0;
        PMIB_UDP6TABLE_OWNER_PID udp6_table = NULL;

        if (g_GetExtendedUdpTable == NULL) {
            get_udp6 = 0;
        } else {
        g_GetExtendedUdpTable(NULL, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
        if (size > 0) {
            udp6_table = (PMIB_UDP6TABLE_OWNER_PID)malloc(size);
        }
        
        if (udp6_table != NULL && g_GetExtendedUdpTable(udp6_table, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
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
                

                if (pid_filter != 0 && udp6_table->table[i].dwOwningPid != (DWORD)pid_filter) {
                    continue;
                }               connections[index].fd = -1;
                connections[index].family = AF_INET6;
                connections[index].type = SOCK_DGRAM;
                format_address6(connections[index].laddr, sizeof(connections[index].laddr), 
                               udp6_table->table[i].ucLocalAddr, udp6_table->table[i].dwLocalPort);
                connections[index].raddr[0] = '\0';
                connections[index].status = PSUTIL_CONN_NONE;
                index++;
            }
        }
        free(udp6_table);
        }
    }
    
    *count = index;
    if (index == 0) {
        free(connections);
        return NULL;
    }
    return connections;
}

psutil_net_if_addr* psutil_windows_net_if_addrs(int* count) {
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_UNSPEC;
    PIP_ADAPTER_ADDRESSES adapterAddresses = NULL;
    psutil_net_if_addr* addrs = NULL;
    ULONG bufferSize = 0;
    DWORD result;
    int capacity = 0;
    int index = 0;
    PIP_ADAPTER_ADDRESSES adapter;

    *count = 0;
    if (g_GetAdaptersAddresses == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_UNSUPPORTED);
        return NULL;
    }

    // GetAdaptersAddresses wants a buffer big enough for *all* adapters; the
    // old code allocated exactly what the first (empty) call reported.
    bufferSize = 16 * 1024;
    for (;;) {
        PIP_ADAPTER_ADDRESSES buf = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (buf == NULL) {
            psutil_set_last_error(PSUTIL_ERROR_MEMORY);
            return NULL;
        }
        result = g_GetAdaptersAddresses(family, flags, NULL, buf, &bufferSize);
        if (result == ERROR_SUCCESS) {
            adapterAddresses = buf;
            break;
        }
        free(buf);
        if (result != ERROR_BUFFER_OVERFLOW) {
            psutil_set_last_error(PSUTIL_ERROR_IO);
            return NULL;
        }
        bufferSize *= 2;
    }

    for (adapter = adapterAddresses; adapter != NULL; adapter = adapter->Next) {
        PIP_ADAPTER_UNICAST_ADDRESS unicast;
        for (unicast = adapter->FirstUnicastAddress; unicast != NULL;
             unicast = unicast->Next) {
            if (index + 2 > capacity) {
                int ncap = (capacity == 0) ? 32 : (capacity * 2);
                psutil_net_if_addr* na = (psutil_net_if_addr*)realloc(
                    addrs, (size_t)ncap * sizeof(psutil_net_if_addr));
                if (na == NULL) {
                    free(addrs);
                    free(adapterAddresses);
                    psutil_set_last_error(PSUTIL_ERROR_MEMORY);
                    return NULL;
                }
                addrs = na;
                capacity = ncap;
            }

            memset(&addrs[index], 0, sizeof(psutil_net_if_addr));
            {
                // FriendlyName is UTF-16; CP_ACP produced mojibake for
                // Chinese adapter names ("以太网" -> garbage).
                char* name = psutil_windows_wide_to_utf8(adapter->FriendlyName, -1);
                if (name != NULL) {
                    snprintf(addrs[index].name, sizeof(addrs[index].name), "%s",
                             name);
                    free(name);
                }
            }

            {
                const SOCKADDR* sa = unicast->Address.lpSockaddr;
                addrs[index].family = sa->sa_family;
                if (sa->sa_family == AF_INET) {
                    const struct sockaddr_in* sin = (const struct sockaddr_in*)sa;
                    uint32_t prefix = unicast->OnLinkPrefixLength;
                    uint32_t host = ntohl(sin->sin_addr.S_un.S_addr);
                    uint32_t mask = 0;
                    uint32_t i;
                    uint32_t bcast;

                    if (prefix > 32) {
                        prefix = 32;
                    }
                    for (i = 0; i < prefix; i++) {
                        mask |= (uint32_t)1 << (31 - i);
                    }
                    snprintf(addrs[index].address, sizeof(addrs[index].address),
                             "%u.%u.%u.%u", (unsigned)((host >> 24) & 0xFF),
                             (unsigned)((host >> 16) & 0xFF),
                             (unsigned)((host >> 8) & 0xFF),
                             (unsigned)(host & 0xFF));
                    psutil_windows_ipv4_mask(prefix, addrs[index].netmask,
                                             sizeof(addrs[index].netmask));
                    bcast = host | ~mask;
                    snprintf(addrs[index].broadcast,
                             sizeof(addrs[index].broadcast),
                             "%u.%u.%u.%u", (unsigned)((bcast >> 24) & 0xFF),
                             (unsigned)((bcast >> 16) & 0xFF),
                             (unsigned)((bcast >> 8) & 0xFF),
                             (unsigned)(bcast & 0xFF));
                } else if (sa->sa_family == AF_INET6) {
                    const struct sockaddr_in6* sin6 = (const struct sockaddr_in6*)sa;
                    const unsigned char* a = sin6->sin6_addr.s6_addr;
                    snprintf(addrs[index].address, sizeof(addrs[index].address),
                             "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                             a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                             a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
                    addrs[index].netmask[0] = '\0';
                    addrs[index].broadcast[0] = '\0';
                } else {
                    continue;
                }
            }
            index++;

            // One AF_PACKET-style row per adapter carries the hardware
            // address, mirroring psutil's per-family ifaddr output.
            if (adapter->PhysicalAddressLength > 0) {
                size_t used = 0;
                DWORD b;
                memset(&addrs[index], 0, sizeof(psutil_net_if_addr));
                addrs[index].family = AF_PACKET;
                snprintf(addrs[index].name, sizeof(addrs[index].name), "%s",
                         addrs[index - 1].name);
                for (b = 0; b < adapter->PhysicalAddressLength &&
                            used + 4 < sizeof(addrs[index].mac); b++) {
                    used += (size_t)snprintf(addrs[index].mac + used,
                                             sizeof(addrs[index].mac) - used,
                                             "%s%02x", (b == 0) ? "" : ":",
                                             adapter->PhysicalAddress[b]);
                }
                addrs[index].address[0] = '\0';
                addrs[index].netmask[0] = '\0';
                addrs[index].broadcast[0] = '\0';
                index++;
            }
        }
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
        
        // wszName is UTF-16: CP_ACP turned Chinese adapter names into mojibake.
        memset(&stats[index], 0, sizeof(psutil_net_if_stat));
        {
            char* if_name = psutil_windows_wide_to_utf8(row->wszName, -1);
            if (if_name != NULL) {
                snprintf(stats[index].name, sizeof(stats[index].name), "%s", if_name);
                free(if_name);
            }
        }

        stats[index].isup = (row->dwOperStatus == IF_OPER_STATUS_OPERATIONAL) ? 1 : 0;
        stats[index].speed = (int)(row->dwSpeed / 1000000); // Convert to Mbps
        stats[index].mtu = row->dwMtu;
        
        // Determine duplex (not available in MIB_IFROW, use unknown)
        stats[index].duplex = PSUTIL_NIC_DUPLEX_UNKNOWN;
        
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
    int found = 0;
    int i;

    (void)perdisk;  // psutil_io_counters has no per-disk breakdown
    // The old code only ever opened PhysicalDrive0, so every other disk was
    // silently missing from the totals. IOCTL_DISK_PERFORMANCE must be
    // requested with 0 access rights, which is what CreateFile below does.
    for (i = 0; i < 64; i++) {
        char path[64];
        HANDLE hDevice;
        DISK_PERFORMANCE perf;
        DWORD bytes_returned = 0;

        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);
        hDevice = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) {
            continue;
        }
        found++;
        memset(&perf, 0, sizeof(perf));
        if (DeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0, &perf,
                            sizeof(perf), &bytes_returned, NULL)) {
            counters.read_bytes += perf.BytesRead.QuadPart;
            counters.write_bytes += perf.BytesWritten.QuadPart;
            counters.read_count += perf.ReadCount;
            counters.write_count += perf.WriteCount;
            // ReadTime/WriteTime are 100ns units; psutil reports milliseconds.
            counters.read_time += (uint64_t)(perf.ReadTime.QuadPart / 10000);
            counters.write_time += (uint64_t)(perf.WriteTime.QuadPart / 10000);
        }
        CloseHandle(hDevice);
    }

    if (found == 0) {
        psutil_set_last_error(PSUTIL_ERROR_PERMISSION);
    }
    return counters;
}
psutil_disk_partition* psutil_windows_disk_partitions(int all, int* count) {
    DWORD drives = GetLogicalDrives();

    if (count == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return NULL;
    }
    *count = 0;
    if (drives == 0) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return NULL;
    }

    // Count partitions first
    int npartitions = 0;
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char drivePath[4] = {'A' + i, ':', '\\', '\0'};
            UINT driveType = GetDriveTypeA(drivePath);
            if (all || driveType == DRIVE_FIXED || driveType == DRIVE_REMOTE) {
                npartitions++;
            }
        }
    }

    if (npartitions == 0) {
        return NULL;
    }

    // Allocate array (extra element for terminator)
    psutil_disk_partition* partitions = (psutil_disk_partition*)malloc((npartitions + 1) * sizeof(psutil_disk_partition));
    if (partitions == NULL) {
        return NULL;
    }

    int index = 0;
    for (int i = 0; i < 26 && index < npartitions; i++) {
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

    // Report how many entries were actually filled in (the old signature
    // forced callers to rely on a NUL-terminated device name).
    *count = index;

    // Mark end of array
    if (index < npartitions + 1) {
        partitions[index].device[0] = '\0';
    }

    return partitions;
}

psutil_disk_usage psutil_windows_disk_usage(const char* path) {
    psutil_disk_usage usage = {0};
    ULARGE_INTEGER free_bytes_available, total_bytes, total_free_bytes;
    WCHAR wpath[MAX_PATH * 2];

    if (path == NULL) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return usage;
    }
    // Paths coming from the rest of the library are UTF-8, so convert instead
    // of calling the A variant (which mis-parses non-ASCII directories).
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath,
                            (int)(sizeof(wpath) / sizeof(WCHAR))) <= 0) {
        psutil_set_last_error(PSUTIL_ERROR_INVALID_ARG);
        return usage;
    }
    if (!GetDiskFreeSpaceExW(wpath, &free_bytes_available, &total_bytes,
                             &total_free_bytes)) {
        psutil_set_last_error(PSUTIL_ERROR_IO);
        return usage;
    }

    usage.total = total_bytes.QuadPart;
    usage.free = total_free_bytes.QuadPart;
    usage.used = (usage.total > usage.free) ? (usage.total - usage.free) : 0;
    if (usage.total > 0) {
        // Dividing by total without this guard produced inf/NaN for empty
        // or not-ready removable drives.
        usage.percent = (double)usage.used / (double)usage.total * 100.0;
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
                memset(&users[index], 0, sizeof(psutil_user));

                // Convert username to char (UTF-8: the rest of this library
                // returns UTF-8, CP_ACP produced garbage for non-ASCII logins)
                {
                    char* user_utf8 = psutil_windows_wide_to_utf8(username, -1);
                    if (user_utf8 != NULL) {
                        snprintf(users[index].name, sizeof(users[index].name),
                                 "%s", user_utf8);
                        free(user_utf8);
                    }
                }

                // Get domain
                if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSDomainName, &domain, &bytesReturned)) {
                    if (domain != NULL && wcslen(domain) > 0) {
                        char domain_str[64];
                        char full_name[sizeof(users[index].name)];
                        char* domain_utf8 = psutil_windows_wide_to_utf8(domain, -1);
                        if (domain_utf8 != NULL) {
                            snprintf(domain_str, sizeof(domain_str), "%s",
                                     domain_utf8);
                            snprintf(full_name, sizeof(full_name), "%s\\%s",
                                     domain_str, users[index].name);
                            snprintf(users[index].name,
                                     sizeof(users[index].name), "%s", full_name);
                            free(domain_utf8);
                        }
                    }
                    g_WTSFreeMemory(domain);
                }
                
                // Get terminal name
                LPWSTR stationName = NULL;
                if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessions[i].SessionId, WTSWinStationName, &stationName, &bytesReturned)) {
                    if (stationName != NULL) {
                        char* term_utf8 = psutil_windows_wide_to_utf8(stationName, -1);
                        if (term_utf8 != NULL) {
                            snprintf(users[index].terminal,
                                     sizeof(users[index].terminal), "%s",
                                     term_utf8);
                            free(term_utf8);
                        }
                        g_WTSFreeMemory(stationName);
                    }
                }
                
                // Logon time: WTSSessionInfo carries the real logon timestamp.
                // The old code substituted time(NULL), i.e. the moment the
                // query ran, which is always wrong.
                users[index].started = (double)time(NULL);
                {
                    PWTSINFO info = NULL;
                    DWORD info_bytes = 0;
                    if (g_WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                                      sessions[i].SessionId,
                                                      WTSSessionInfo, (LPWSTR*)&info,
                                                      &info_bytes) &&
                        info != NULL) {
                        FILETIME ft;
                        ft.dwLowDateTime = (DWORD)(info->LogonTime.QuadPart & 0xFFFFFFFFu);
                        ft.dwHighDateTime = (DWORD)(info->LogonTime.QuadPart >> 32);
                        users[index].started = psutil_FiletimeToUnixTime(ft);
                        g_WTSFreeMemory(info);
                    }
                }
                
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













