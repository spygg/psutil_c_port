cmake_minimum_required(VERSION 3.10)
project(psutil C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

message(STATUS "Building psutil project")

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Build libpsutil library
add_subdirectory(libpsutil)

# Example executable
add_executable(example example.c)
target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(example psutil)

# Link platform-specific libraries to example
if(WIN32)
    # MSVC and MinGW both need the Windows libraries exported from libpsutil
    if(PSUTIL_LIBRARIES)
        target_link_libraries(example ${PSUTIL_LIBRARIES})
    endif()
elseif(ANDROID)
    # Android Bionic libc includes pthread, no need to link separately
    # Some functions may require log library
    target_link_libraries(example log)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(example pthread)
    # Link rt library for CPU affinity functions
    target_link_libraries(example rt)
elseif(APPLE)
    # macOS may need additional frameworks
    # (handled by PSUTIL_LIBRARIES if set)
    if(PSUTIL_LIBRARIES)
        target_link_libraries(example ${PSUTIL_LIBRARIES})
    endif()
endif()

# MSVC: suppress CRT warnings (e.g. strncpy, sscanf)
if(MSVC)
    target_compile_definitions(example PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(psutil PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# MinGW: ensure we use the correct Windows API headers
if(MINGW)
    target_compile_definitions(example PRIVATE _WIN32_WINNT=0x0501)
    target_compile_definitions(psutil PRIVATE _WIN32_WINNT=0x0501)
endif()

# Installation
install(TARGETS example
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: psutil")
message(STATUS "  Example executable: example")
message(STATUS "")
