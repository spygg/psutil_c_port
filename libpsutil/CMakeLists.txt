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
elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|NetBSD|OpenBSD|DragonFly")
    set(PSUTIL_PLATFORM "bsd")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_BSD")
    set(PSUTIL_PLATFORM_DIR "arch/bsd")
    set(PSUTIL_PLATFORM_SOURCES "arch/bsd/psutil_bsd.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/bsd/psutil_bsd.h")
    # BSD may need kvm for process listing
    set(PSUTIL_LIBRARIES "")
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

target_compile_definitions(psutil PRIVATE ${PSUTIL_PLATFORM_DEFINE})

# MSVC: suppress CRT warnings (strncpy, sscanf, strdup, etc.)
if(MSVC)
    target_compile_definitions(psutil PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    # MSVC doesn't support C99 fully, but we can use C11
    set_target_properties(psutil PROPERTIES C_STANDARD 11)
endif()

# MinGW: target Windows XP (0x0501) for maximum compatibility
if(MINGW)
    target_compile_definitions(psutil PRIVATE _WIN32_WINNT=0x0501 WINVER=0x0501)
    # MinGW needs _GNU_SOURCE for some functions like strdup
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

# Installation
install(TARGETS psutil
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES psutil.h ${PSUTIL_PLATFORM_HEADERS}
    DESTINATION include/psutil
)

# Export platform-specific variables to parent scope
set(PSUTIL_LIBRARIES ${PSUTIL_LIBRARIES} PARENT_SCOPE)
set(PSUTIL_PLATFORM_DEFINE ${PSUTIL_PLATFORM_DEFINE} PARENT_SCOPE)
