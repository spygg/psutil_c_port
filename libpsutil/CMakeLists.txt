cmake_minimum_required(VERSION 3.10)

# Platform detection
if(WIN32)
    set(PSUTIL_PLATFORM "windows")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_WINDOWS")
    set(PSUTIL_PLATFORM_DIR "arch/windows")
    set(PSUTIL_PLATFORM_SOURCES "arch/windows/psutil_windows.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/windows/psutil_windows.h")
    # Windows libraries needed for process/system info
    # psapi: process info (EnumProcesses, GetModuleFileNameEx, etc.)
    # iphlpapi: network interface info (GetIfTable, GetAdaptersAddresses, etc.)
    # wtsapi32: terminal services (WTSEnumerateSessions, etc.)
    # advapi32: security (OpenProcessToken, LookupAccountSid, etc.)
    # ws2_32: Winsock (inet_ntoa, etc.)
    # userenv: environment block support
    set(PSUTIL_LIBRARIES psapi iphlpapi wtsapi32 advapi32 ws2_32 userenv)
elseif(APPLE)
    set(PSUTIL_PLATFORM "macos")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_MACOS")
    set(PSUTIL_PLATFORM_DIR "arch/macos")
    set(PSUTIL_PLATFORM_SOURCES "arch/macos/psutil_macos.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/macos/psutil_macos.h")
    set(PSUTIL_LIBRARIES "")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    # The BSD backend is FreeBSD-specific (kvm_getprocs, sysctl KERN_PROC,
    # kinfo_proc layout). It does not build on NetBSD/OpenBSD/DragonFly, so
    # fail loudly instead of producing a library that cannot link.
    set(PSUTIL_PLATFORM "bsd")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_BSD")
    set(PSUTIL_PLATFORM_DIR "arch/bsd")
    set(PSUTIL_PLATFORM_SOURCES "arch/bsd/psutil_bsd.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/bsd/psutil_bsd.h")
    set(PSUTIL_LIBRARIES "kvm")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD|OpenBSD|DragonFly")
    message(FATAL_ERROR "libpsutil: only FreeBSD is supported in arch/bsd "
                        "(${CMAKE_SYSTEM_NAME} needs its own backend)")
elseif(ANDROID)
    # Android must be checked before Linux since Android also defines CMAKE_SYSTEM_NAME as Linux
    set(PSUTIL_PLATFORM "android")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_ANDROID")
    set(PSUTIL_PLATFORM_DIR "arch/android")
    set(PSUTIL_PLATFORM_SOURCES "arch/android/psutil_android.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/android/psutil_android.h")
    set(PSUTIL_LIBRARIES "")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(PSUTIL_PLATFORM "linux")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_LINUX")
    set(PSUTIL_PLATFORM_DIR "arch/linux")
    set(PSUTIL_PLATFORM_SOURCES "arch/linux/psutil_linux.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/linux/psutil_linux.h")
    set(PSUTIL_LIBRARIES "")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
    set(PSUTIL_PLATFORM "unknown")
    set(PSUTIL_PLATFORM_DEFINE "")
    set(PSUTIL_PLATFORM_DIR "")
    set(PSUTIL_PLATFORM_SOURCES "")
    set(PSUTIL_PLATFORM_HEADERS "")
    set(PSUTIL_LIBRARIES "")
endif()

message(STATUS "libpsutil - Detected platform: ${PSUTIL_PLATFORM}")
message(STATUS "libpsutil - Platform define: ${PSUTIL_PLATFORM_DEFINE}")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/arch)

# psutil library
add_library(psutil STATIC
    psutil.c
    ${PSUTIL_PLATFORM_SOURCES}
)

# PUBLIC on purpose: consumers that include psutil.h compile the same inline
# branches as the library, so the platform macro must not be library-private.
target_compile_definitions(psutil PUBLIC ${PSUTIL_PLATFORM_DEFINE})

# MSVC: suppress CRT warnings (strncpy, sscanf, strdup, etc.)
if(MSVC)
    target_compile_definitions(psutil PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    # MSVC doesn't support C99 fully, but we can use C11
    set_target_properties(psutil PROPERTIES C_STANDARD 11)
endif()

# Windows: the code uses Vista+ APIs (PROCESS_QUERY_LIMITED_INFORMATION,
# GetTickCount64, GetLogicalProcessorInformationEx), so 0x0501 (XP) hid
# declarations and produced implicit-declaration errors.
if(WIN32)
    target_compile_definitions(psutil PRIVATE _WIN32_WINNT=0x0600 WINVER=0x0600)
endif()
if(MINGW)
    target_compile_definitions(psutil PRIVATE _GNU_SOURCE)
endif()

# Linux: _GNU_SOURCE exposes sched_getaffinity/sched_setaffinity and
# non-standard utmp members
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(psutil PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
endif()

if(PSUTIL_LIBRARIES)
    target_link_libraries(psutil ${PSUTIL_LIBRARIES})
endif()

# Warnings on GCC/Clang. -Wextra is what caught several of the fixed bugs
# (uninitialised counters, unused parameters).
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(psutil PRIVATE -Wall -Wextra)
    set_target_properties(psutil PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
endif()

# Installation
install(TARGETS psutil
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Only the public header is installed. arch/*.h are internal: they include
# "../../psutil.h" and would be broken once copied into include/psutil.
install(FILES psutil.h DESTINATION include/psutil)

# Export platform-specific variables to parent scope
set(PSUTIL_LIBRARIES ${PSUTIL_LIBRARIES} PARENT_SCOPE)
set(PSUTIL_PLATFORM_DEFINE ${PSUTIL_PLATFORM_DEFINE} PARENT_SCOPE)

