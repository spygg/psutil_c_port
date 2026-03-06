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
│       ├── macos/        # macOS implementation
│       └── bsd/          # BSD implementation
├── example.c            # Example usage
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

### Platform-Specific Notes

- **Windows**: Requires MinGW or Visual Studio
- **Linux**: Requires gcc and libc-dev
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

## License

This project is licensed under the BSD License. See the LICENSE file for details.

---

