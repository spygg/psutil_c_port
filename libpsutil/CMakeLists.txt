cmake_minimum_required(VERSION 3.10)

# Platform detection
if(WIN32)
    set(PSUTIL_PLATFORM "windows")
    set(PSUTIL_PLATFORM_DEFINE "PSUTIL_WINDOWS")
    set(PSUTIL_PLATFORM_DIR "arch/windows")
    set(PSUTIL_PLATFORM_SOURCES "arch/windows/psutil_windows.c")
    set(PSUTIL_PLATFORM_HEADERS "arch/windows/psutil_windows.h")
    set(PSUTIL_LIBRARIES psapi iphlpapi wtsapi32 advapi32 ws2_32)
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
