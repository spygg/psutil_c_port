# psutil C 库

这是 psutil 的 C 语言实现，一个跨平台库，用于检索运行进程和系统利用率的信息。该项目从原始的 psutil Python 库移植而来，并在 AI 的协助下完成。

## English Version
[Click here to view the English version](README_EN.md)


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
├── README.md            # 中文版本
└── README_EN.md         # 英文版本
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
- ✅ 进程信息（名称、可执行文件、命令行、状态等）
- ✅ 进程内存和 CPU 使用情况
- ✅ 系统内存和 CPU 信息
- ✅ 磁盘分区和使用情况
- ✅ 网络连接和 IO 统计
- ✅ 进程控制（挂起、恢复、终止、杀死）
- ✅ 系统启动时间和运行时间
- ✅ 网络接口地址

## TODO 列表

### 通用
- [ ] 添加全面的单元测试
- [ ] 改进错误处理
- [ ] 为所有函数添加文档
- [ ] 优化大进程列表的性能

### macOS 平台
- [ ] 编译和测试

### BSD 平台
- [ ] 编译和测试

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
- **macOS**：需要 Xcode 命令行工具（注：因环境限制，未进行编译和测试）
- **BSD**：需要适当的开发工具（注：因环境限制，未进行编译和测试）

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
