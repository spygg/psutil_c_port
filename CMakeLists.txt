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

# Link pthread library for Linux
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(example pthread)
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
