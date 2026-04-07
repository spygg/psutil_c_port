# psutil C Library

This is the C implementation of psutil, a cross-platform library for retrieving information on running processes and system utilization. This project is ported from the original psutil Python library and completed with the assistance of AI.

## Chinese Version
[中文版](README.md)

## Project Origin

This C library is a port of the [psutil](https://github.com/giampaolo/psutil) Python library, which was created by Giampaolo Rodola'. The C implementation aims to provide the same functionality as the original Python version but with the performance benefits of C.

## Development Approach

The development of this C library was accelerated using AI assistance, which helped with:
- Porting Python code to C
- Implementing platform-specific functions
- Debugging and error handling
- Optimizing performance
- Ensuring cross-platform compatibility

## Project Structure

```
├── libpsutil/           # Main library code
│   ├── psutil.c         # Core implementation
│   ├── psutil.h         # Header file
│   ├── CMakeLists.txt   # Library CMake configuration
│   └── arch/            # Platform-specific implementations
│       ├── windows/      # Windows implementation
│       ├── linux/        # Linux implementation
│       ├── android/      # Android implementation
│       ├── macos/        # macOS implementation
│       └── bsd/          # BSD implementation
├── example.c            # C example usage
├── example.py           # Python example usage
├── CMakeLists.txt       # Main CMake configuration
├── README.md            # Chinese version
└── README_EN.md         # English version
```

## Implemented Features

### Windows Platform
- ✅ Process information (name, exe, cmdline, status, etc.)
- ✅ Process memory and CPU usage
- ✅ System memory and CPU information
- ✅ Disk partitions and usage
- ✅ Network connections and IO statistics
- ✅ Process control (suspend, resume, terminate, kill)
- ✅ System boot time and uptime
- ✅ Network interface addresses

### Linux Platform
- ✅ Process information (name, exe, cmdline, status, etc.)
- ✅ Process memory and CPU usage
- ✅ System memory and CPU information
- ✅ Disk partitions and usage
- ✅ Network connections and IO statistics
- ✅ Process control (suspend, resume, terminate, kill)
- ✅ System boot time and uptime
- ✅ Network interface addresses

### macOS Platform
- ✅ Process information (name, exe, cmdline, status, etc.)
- ✅ Process memory and CPU usage
- ✅ System memory and CPU information
- ✅ Disk partitions and usage
- ✅ Network connections and IO statistics
- ✅ Process control (suspend, resume, terminate, kill)
- ✅ System boot time and uptime
- ✅ Network interface addresses

### Android Platform
- ✅ Process information (name, exe, cmdline, status, etc.)
- ✅ Process memory and CPU usage
- ✅ System memory and CPU information
- ✅ Disk partitions and usage
- ✅ Network connections and IO statistics
- ✅ Process control (suspend, resume, terminate, kill)
- ✅ System boot time and uptime
- ✅ Network interface addresses

### BSD Platform
- ✅ Process information (name, exe, cmdline, status, etc.)
- ✅ Process memory and CPU usage
- ✅ System memory and CPU information
- ✅ Disk partitions and usage
- ✅ Network connections and IO statistics
- ✅ Process control (suspend, resume, terminate, kill)
- ✅ System boot time and uptime
- ✅ Network interface addresses

## TODO List

### General
- [ ] Add comprehensive unit tests
- [ ] Improve error handling
- [ ] Add documentation for all functions
- [ ] Optimize performance for large process lists

### macOS Platform
- [ ] Compile and test

### Android Platform
- [ ] Compile and test with NDK

### BSD Platform
- [ ] Compile and test

## Build Instructions

### Using CMake

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Configure the project:
   ```bash
   cmake ..
   ```

3. Build the library and example:
   ```bash
   cmake --build .
   ```

4. Run the example:
   ```bash
   ./example
   ```

### Android NDK Build

1. Set NDK environment variable:
   ```bash
   export ANDROID_NDK=/path/to/android-ndk
   ```

2. Create toolchain file `android-toolchain.cmake`:
   ```cmake
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 21)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
   set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK})
   set(CMAKE_ANDROID_STL_TYPE c++_static)
   ```

3. Configure and build:
   ```bash
   mkdir build-android
   cd build-android
   cmake -DCMAKE_TOOLCHAIN_FILE=../android-toolchain.cmake ..
   cmake --build .
   ```

### Platform-Specific Notes

- **Windows**: Requires MinGW or Visual Studio
- **Linux**: Requires gcc and libc-dev
- **Android**: Requires Android NDK and CMake (API 21+)
- **macOS**: Requires Xcode command line tools (Note: Not compiled and tested due to environment limitations)
- **BSD**: Requires appropriate development tools (Note: Not compiled and tested due to environment limitations)

## Usage Example

```c
#include "libpsutil/psutil.h"
#include <stdio.h>

int main() {
    // Initialize the library
    if (psutil_init() != 0) {
        printf("Failed to initialize psutil\n");
        return 1;
    }

    // Get current process
    Process* proc = process_new(getpid());
    if (proc) {
        printf("Process name: %s\n", process_get_name(proc));
        printf("Process exe: %s\n", process_get_exe(proc));
        process_free(proc);
    }

    // Get system memory info
    psutil_memory_info mem = virtual_memory();
    printf("Total memory: %llu bytes\n", mem.total);
    printf("Available memory: %llu bytes\n", mem.available);

    // Get CPU count
    int cpu_count = cpu_count(1); // 1 = logical cores
    printf("CPU count: %d\n", cpu_count);

    // Cleanup
    psutil_cleanup();
    return 0;
}
```

For more detailed examples, please refer to the `example.c` file.

## API Reference

### Library Initialization and Cleanup

| Function | Description | Return Value |
|----------|-------------|--------------|
| `int psutil_init(void)` | Initialize psutil library | 0 for success, non-zero for failure |

### Process Operations

#### Process Object Management
| Function | Description |
|----------|-------------|
| `Process* process_new(uint32_t pid)` | Create Process object (pid=0 for current process) |
| `void process_free(Process* proc)` | Free Process object |
| `uint32_t process_get_pid(Process* proc)` | Get process PID |
| `uint32_t process_get_ppid(Process* proc)` | Get parent process PID |
| `const char* process_get_name(Process* proc)` | Get process name |
| `const char* process_get_exe(Process* proc)` | Get executable path |
| `char** process_get_cmdline(Process* proc, int* count)` | Get command line arguments |
| `int process_get_status(Process* proc)` | Get process status |
| `const char* process_get_username(Process* proc)` | Get process username |
| `double process_get_create_time(Process* proc)` | Get process creation time |
| `const char* process_get_cwd(Process* proc)` | Get current working directory |

#### Process Resource Usage
| Function | Description |
|----------|-------------|
| `psutil_memory_info process_get_memory_info(Process* proc)` | Get memory information |
| `psutil_memory_info process_get_memory_full_info(Process* proc)` | Get full memory info (with USS/PSS) |
| `double process_get_memory_percent(Process* proc, const char* memtype)` | Get memory usage percentage |
| `psutil_cpu_times process_get_cpu_times(Process* proc)` | Get CPU time statistics |
| `int process_get_num_threads(Process* proc)` | Get thread count |
| `psutil_thread* process_get_threads(Process* proc, int* count)` | Get thread list |
| `psutil_io_counters process_get_io_counters(Process* proc)` | Get I/O statistics |
| `psutil_ctx_switches process_get_num_ctx_switches(Process* proc)` | Get context switch count |

#### Process Control
| Function | Description | Return Value |
|----------|-------------|--------------|
| `int process_send_signal(Process* proc, int sig)` | Send signal | 0 for success |
| `int process_suspend(Process* proc)` | Suspend process | 0 for success |
| `int process_resume(Process* proc)` | Resume process | 0 for success |
| `int process_terminate(Process* proc)` | Terminate process | 0 for success |
| `int process_kill(Process* proc)` | Kill process | 0 for success |
| `int process_is_running(Process* proc)` | Check if process is running | 1 if running |

#### Process Priority
| Function | Description |
|----------|-------------|
| `int process_get_nice(Process* proc)` | Get process nice value |
| `int process_set_nice(Process* proc, int value)` | Set process nice value |
| `int process_get_ionice(Process* proc)` | Get I/O priority |
| `int process_set_ionice(Process* proc, int ioclass, int value)` | Set I/O priority |
| `int* process_get_cpu_affinity(Process* proc, int* count)` | Get CPU affinity |
| `int process_set_cpu_affinity(Process* proc, int* cpus, int count)` | Set CPU affinity |

### System Information

#### CPU Information
| Function | Description |
|----------|-------------|
| `int cpu_count(int logical)` | Get CPU core count (1=logical, 0=physical) |
| `psutil_cpu_times cpu_times(int percpu)` | Get CPU time statistics |
| `double cpu_percent(double interval, int percpu)` | Get CPU usage percentage |
| `psutil_cpu_stats cpu_stats()` | Get CPU statistics |

#### Memory Information
| Function | Description |
|----------|-------------|
| `psutil_memory_info virtual_memory()` | Get virtual memory information |
| `psutil_memory_info swap_memory()` | Get swap memory information |

#### Disk Information
| Function | Description |
|----------|-------------|
| `psutil_disk_partition* disk_partitions(int all)` | Get disk partition list |
| `psutil_disk_usage disk_usage(const char* path)` | Get disk usage for path |
| `psutil_io_counters disk_io_counters(int perdisk)` | Get disk I/O statistics |

#### Network Information
| Function | Description |
|----------|-------------|
| `psutil_io_counters net_io_counters(int pernic)` | Get network I/O statistics |
| `psutil_net_if_addr* net_if_addrs(int* count)` | Get network interface addresses |
| `psutil_net_if_stat* net_if_stats(int* count)` | Get network interface statistics |
| `psutil_net_connection* net_connections(const char* kind, int* count)` | Get network connections |

#### Other System Information
| Function | Description |
|----------|-------------|
| `uint32_t* pids(int* count)` | Get list of all running process PIDs |
| `int pid_exists(uint32_t pid)` | Check if PID exists |
| `double boot_time()` | Get system boot time |
| `psutil_user* users(int* count)` | Get logged in users |

### Data Structures

#### psutil_memory_info
```c
typedef struct {
    uint64_t rss;      // Resident Set Size
    uint64_t vms;      // Virtual Memory Size
    uint64_t shared;   // Shared memory
    uint64_t text;     // Text segment size
    uint64_t lib;      // Library size
    uint64_t data;     // Data segment size
    uint64_t dirty;    // Dirty pages
    uint64_t uss;      // Unique Set Size (Linux)
    uint64_t pss;      // Proportional Set Size (Linux)
} psutil_memory_info;
```

#### psutil_cpu_times
```c
typedef struct {
    double user;            // User time
    double system;          // System time
    double children_user;   // Children user time
    double children_system; // Children system time
} psutil_cpu_times;
```

#### psutil_io_counters
```c
typedef struct {
    uint64_t read_count;   // Read operations count
    uint64_t write_count;  // Write operations count
    uint64_t read_bytes;   // Bytes read
    uint64_t write_bytes;  // Bytes written
    uint64_t read_time;    // Read time
    uint64_t write_time;   // Write time
} psutil_io_counters;
```

### Constants

#### Process Status
```c
#define STATUS_RUNNING      0   // Running
#define STATUS_IDLE         1   // Idle
#define STATUS_SLEEPING     2   // Sleeping
#define STATUS_DISK_SLEEP   3   // Uninterruptible sleep
#define STATUS_STOPPED      4   // Stopped
#define STATUS_TRACING_STOP 5   // Tracing stop
#define STATUS_ZOMBIE       6   // Zombie
#define STATUS_DEAD         7   // Dead
#define STATUS_WAKING       8   // Waking
#define STATUS_LOCKED       9   // Locked
#define STATUS_WAITING     10   // Waiting
#define STATUS_PARKED      11   // Parked
```

## License

This project is licensed under the BSD License. See the LICENSE file for details.

---

