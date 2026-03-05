# psutil C 库

这是 psutil 的 C 语言实现，一个跨平台库，用于检索运行进程和系统利用率的信息。该项目从原始的 psutil Python 库移植而来，并在 AI 的协助下完成。

## 项目来源

这个 C 库是 [psutil](https://github.com/giampaolo/psutil) Python 库的移植版本，原始库由 Giampaolo Rodola' 创建。C 实现旨在提供与原始 Python 版本相同的功能，但具有 C 语言的性能优势。

## 开发方法

这个 C 库的开发通过 AI 协助加速完成，AI 帮助：
- 将 Python 代码移植到 C
- 实现平台特定功能
- 调试和错误处理
- 性能优化
- 确保跨平台兼容性

## 项目结构

```
├── libpsutil/           # 主库代码
│   ├── psutil.c         # 核心实现
│   ├── psutil.h         # 头文件
│   ├── CMakeLists.txt   # 库 CMake 配置
│   └── arch/            # 平台特定实现
│       ├── windows/      # Windows 实现
│       ├── linux/        # Linux 实现
│       ├── macos/        # macOS 实现
│       └── bsd/          # BSD 实现
├── example.c            # 使用示例
├── CMakeLists.txt       # 主 CMake 配置
└── README.md            # 本文档
```

## 已实现功能

### Windows 平台
- ✅ 进程信息（名称、可执行文件、命令行、状态等）
- ✅ 进程内存和 CPU 使用情况
- ✅ 系统内存和 CPU 信息
- ✅ 磁盘分区和使用情况
- ✅ 网络连接和 IO 统计
- ✅ 进程控制（挂起、恢复、终止、杀死）
- ✅ 系统启动时间和运行时间
- ✅ 网络接口地址

### Linux 平台
- ✅ 进程信息（名称、可执行文件、命令行、状态等）
- ✅ 进程内存和 CPU 使用情况
- ✅ 系统内存和 CPU 信息
- ✅ 磁盘分区和使用情况
- ✅ 网络连接和 IO 统计
- ✅ 进程控制（挂起、恢复、终止、杀死）
- ✅ 系统启动时间和运行时间
- ✅ 网络接口地址

### macOS 平台
- ✅ 进程信息（名称、可执行文件、命令行、状态等）
- ✅ 进程内存和 CPU 使用情况
- ✅ 系统内存和 CPU 信息
- ✅ 磁盘分区和使用情况
- ✅ 网络连接和 IO 统计
- ✅ 进程控制（挂起、恢复、终止、杀死）
- ✅ 系统启动时间和运行时间
- ✅ 网络接口地址

### BSD 平台
- ✅ 基本框架和结构
- ✅ 进程信息检索
- ✅ 系统信息检索

## TODO 列表

### 通用
- [ ] 添加全面的单元测试
- [ ] 改进错误处理
- [ ] 为所有函数添加文档
- [ ] 优化大进程列表的性能

### macOS 平台
- [ ] `process_get_io_counters` - 进程 IO 统计（需要更复杂的实现）
- [ ] `process_get_ionice/set_ionice` - IO 优先级（macOS 特定实现）
- [ ] `process_get_cpu_affinity/set_cpu_affinity` - CPU 亲和性（macOS 特定实现）
- [ ] `process_get_environ` - 环境变量（需要更复杂的实现）
- [ ] `process_get_memory_maps` - 内存映射（需要 Mach VM API）
- [ ] `process_get_open_files` - 打开的文件（需要 proc_pidinfo 遍历）
- [ ] `process_get_net_connections` - 进程网络连接
- [ ] `cpu_percent` - CPU 使用率百分比（需要两点采样）
- [ ] `cpu_stats` - CPU 统计信息

### BSD 平台
- [ ] 完成磁盘 IO 计数器实现
- [ ] 完成网络 IO 计数器实现
- [ ] 实现进程 IO 计数器
- [ ] 实现进程环境变量

## 构建说明

### 使用 CMake

1. 创建构建目录：
   ```bash
   mkdir build
   cd build
   ```

2. 配置项目：
   ```bash
   cmake ..
   ```

3. 构建库和示例：
   ```bash
   cmake --build .
   ```

4. 运行示例：
   ```bash
   ./example
   ```

### 平台特定说明

- **Windows**：需要 MinGW 或 Visual Studio
- **Linux**：需要 gcc 和 libc-dev
- **macOS**：需要 Xcode 命令行工具
- **BSD**：需要适当的开发工具

## 使用示例

```c
#include "libpsutil/psutil.h"
#include <stdio.h>

int main() {
    // 初始化库
    if (psutil_init() != 0) {
        printf("Failed to initialize psutil\n");
        return 1;
    }

    // 获取当前进程
    Process* proc = process_new(getpid());
    if (proc) {
        printf("Process name: %s\n", process_get_name(proc));
        printf("Process exe: %s\n", process_get_exe(proc));
        process_free(proc);
    }

    // 获取系统内存信息
    psutil_memory_info mem = virtual_memory();
    printf("Total memory: %llu bytes\n", mem.total);
    printf("Available memory: %llu bytes\n", mem.available);

    // 获取 CPU 数量
    int cpu_count = cpu_count(1); // 1 = 逻辑核心
    printf("CPU count: %d\n", cpu_count);

    // 清理
    psutil_cleanup();
    return 0;
}
```

## 许可证

本项目使用 BSD 许可证。详见 LICENSE 文件。

---

## English Version

[Click here to view the English version](#psutil-c-library)

# psutil C Library

This is the C implementation of psutil, a cross-platform library for retrieving information on running processes and system utilization. This project is ported from the original psutil Python library and completed with the assistance of AI.

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
└── README.md            # This file
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
- ✅ Basic framework and structure
- ✅ Process information retrieval
- ✅ System information retrieval

## TODO List

### General
- [ ] Add comprehensive unit tests
- [ ] Improve error handling
- [ ] Add documentation for all functions
- [ ] Optimize performance for large process lists

### macOS Platform
- [ ] `process_get_io_counters` - Process IO statistics (requires more complex implementation)
- [ ] `process_get_ionice/set_ionice` - IO priority (macOS-specific implementation)
- [ ] `process_get_cpu_affinity/set_cpu_affinity` - CPU affinity (macOS-specific implementation)
- [ ] `process_get_environ` - Environment variables (requires more complex implementation)
- [ ] `process_get_memory_maps` - Memory maps (requires Mach VM API)
- [ ] `process_get_open_files` - Open files (requires proc_pidinfo traversal)
- [ ] `process_get_net_connections` - Per-process network connections
- [ ] `cpu_percent` - CPU usage percentage (requires two-point sampling)
- [ ] `cpu_stats` - CPU statistics information

### BSD Platform
- [ ] Complete disk IO counters implementation
- [ ] Complete network IO counters implementation
- [ ] Implement process IO counters
- [ ] Implement process environment variables

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
- **macOS**: Requires Xcode command line tools
- **BSD**: Requires appropriate development tools

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
