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
│       ├── android/      # Android 实现
│       ├── macos/        # macOS 实现
│       └── bsd/          # BSD 实现
├── example.c            # 使用示例
├── example.py           # Python 使用示例
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

### Android 平台
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

### Android 平台
- [ ] 使用 NDK 编译和测试

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

### Android NDK 构建

1. 设置 NDK 环境变量：
   ```bash
   export ANDROID_NDK=/path/to/android-ndk
   ```

2. 创建工具链文件 `android-toolchain.cmake`：
   ```cmake
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 21)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
   set(CMAKE_ANDROID_NDK $ENV{ANDROID_NDK})
   set(CMAKE_ANDROID_STL_TYPE c++_static)
   ```

3. 配置并构建：
   ```bash
   mkdir build-android
   cd build-android
   cmake -DCMAKE_TOOLCHAIN_FILE=../android-toolchain.cmake ..
   cmake --build .
   ```

### 平台特定说明

- **Windows**：需要 MinGW 或 Visual Studio
- **Linux**：需要 gcc 和 libc-dev
- **Android**：需要 Android NDK 和 CMake（API 21+）
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

更多详细示例请参考 `example.c` 文件。

## API 参考

### 库初始化与清理

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `int psutil_init(void)` | 初始化 psutil 库 | 0 表示成功，非 0 表示失败 |

### 进程操作

#### Process 对象管理
| 函数 | 说明 |
|------|------|
| `Process* process_new(uint32_t pid)` | 创建 Process 对象（pid=0 表示当前进程） |
| `void process_free(Process* proc)` | 释放 Process 对象 |
| `uint32_t process_get_pid(Process* proc)` | 获取进程 PID |
| `uint32_t process_get_ppid(Process* proc)` | 获取父进程 PID |
| `const char* process_get_name(Process* proc)` | 获取进程名称 |
| `const char* process_get_exe(Process* proc)` | 获取可执行文件路径 |
| `char** process_get_cmdline(Process* proc, int* count)` | 获取命令行参数 |
| `int process_get_status(Process* proc)` | 获取进程状态 |
| `const char* process_get_username(Process* proc)` | 获取进程用户名 |
| `double process_get_create_time(Process* proc)` | 获取进程创建时间 |
| `const char* process_get_cwd(Process* proc)` | 获取当前工作目录 |

#### 进程资源使用
| 函数 | 说明 |
|------|------|
| `psutil_memory_info process_get_memory_info(Process* proc)` | 获取内存信息 |
| `psutil_memory_info process_get_memory_full_info(Process* proc)` | 获取完整内存信息（含 USS/PSS） |
| `double process_get_memory_percent(Process* proc, const char* memtype)` | 获取内存使用百分比 |
| `psutil_cpu_times process_get_cpu_times(Process* proc)` | 获取 CPU 时间统计 |
| `int process_get_num_threads(Process* proc)` | 获取线程数 |
| `psutil_thread* process_get_threads(Process* proc, int* count)` | 获取线程列表 |
| `psutil_io_counters process_get_io_counters(Process* proc)` | 获取 I/O 统计 |
| `psutil_ctx_switches process_get_num_ctx_switches(Process* proc)` | 获取上下文切换次数 |

#### 进程控制
| 函数 | 说明 | 返回值 |
|------|------|--------|
| `int process_send_signal(Process* proc, int sig)` | 发送信号 | 0 表示成功 |
| `int process_suspend(Process* proc)` | 挂起进程 | 0 表示成功 |
| `int process_resume(Process* proc)` | 恢复进程 | 0 表示成功 |
| `int process_terminate(Process* proc)` | 终止进程 | 0 表示成功 |
| `int process_kill(Process* proc)` | 强制结束进程 | 0 表示成功 |
| `int process_is_running(Process* proc)` | 检查进程是否在运行 | 1 表示运行中 |

#### 进程优先级
| 函数 | 说明 |
|------|------|
| `int process_get_nice(Process* proc)` | 获取进程优先级（nice 值） |
| `int process_set_nice(Process* proc, int value)` | 设置进程优先级 |
| `int process_get_ionice(Process* proc)` | 获取 I/O 优先级 |
| `int process_set_ionice(Process* proc, int ioclass, int value)` | 设置 I/O 优先级 |
| `int* process_get_cpu_affinity(Process* proc, int* count)` | 获取 CPU 亲和性 |
| `int process_set_cpu_affinity(Process* proc, int* cpus, int count)` | 设置 CPU 亲和性 |

### 系统信息

#### CPU 信息
| 函数 | 说明 |
|------|------|
| `int cpu_count(int logical)` | 获取 CPU 核心数（1=逻辑核心，0=物理核心） |
| `psutil_cpu_times cpu_times(int percpu)` | 获取 CPU 时间统计 |
| `double cpu_percent(double interval, int percpu)` | 获取 CPU 使用率 |
| `psutil_cpu_stats cpu_stats()` | 获取 CPU 统计信息 |

#### 内存信息
| 函数 | 说明 |
|------|------|
| `psutil_memory_info virtual_memory()` | 获取虚拟内存信息 |
| `psutil_memory_info swap_memory()` | 获取交换内存信息 |

#### 磁盘信息
| 函数 | 说明 |
|------|------|
| `psutil_disk_partition* disk_partitions(int all)` | 获取磁盘分区列表 |
| `psutil_disk_usage disk_usage(const char* path)` | 获取指定路径的磁盘使用情况 |
| `psutil_io_counters disk_io_counters(int perdisk)` | 获取磁盘 I/O 统计 |

#### 网络信息
| 函数 | 说明 |
|------|------|
| `psutil_io_counters net_io_counters(int pernic)` | 获取网络 I/O 统计 |
| `psutil_net_if_addr* net_if_addrs(int* count)` | 获取网络接口地址 |
| `psutil_net_if_stat* net_if_stats(int* count)` | 获取网络接口状态 |
| `psutil_net_connection* net_connections(const char* kind, int* count)` | 获取网络连接 |

#### 其他系统信息
| 函数 | 说明 |
|------|------|
| `uint32_t* pids(int* count)` | 获取所有运行中的进程 PID 列表 |
| `int pid_exists(uint32_t pid)` | 检查 PID 是否存在 |
| `double boot_time()` | 获取系统启动时间 |
| `psutil_user* users(int* count)` | 获取登录用户信息 |

### 数据结构

#### psutil_memory_info
```c
typedef struct {
    uint64_t rss;      // 实际使用内存
    uint64_t vms;      // 虚拟内存大小
    uint64_t shared;   // 共享内存
    uint64_t text;     // 代码段大小
    uint64_t lib;      // 库大小
    uint64_t data;     // 数据段大小
    uint64_t dirty;    // 脏页大小
    uint64_t uss;      // 唯一集大小（Linux）
    uint64_t pss;      // 比例集大小（Linux）
} psutil_memory_info;
```

#### psutil_cpu_times
```c
typedef struct {
    double user;           // 用户态时间
    double system;         // 内核态时间
    double children_user;  // 子进程用户态时间
    double children_system;// 子进程内核态时间
} psutil_cpu_times;
```

#### psutil_io_counters
```c
typedef struct {
    uint64_t read_count;   // 读取次数
    uint64_t write_count;  // 写入次数
    uint64_t read_bytes;   // 读取字节数
    uint64_t write_bytes;  // 写入字节数
    uint64_t read_time;    // 读取时间
    uint64_t write_time;   // 写入时间
} psutil_io_counters;
```

### 常量定义

#### 进程状态
```c
#define STATUS_RUNNING      0   // 运行中
#define STATUS_IDLE         1   // 空闲
#define STATUS_SLEEPING     2   // 睡眠
#define STATUS_DISK_SLEEP   3   // 不可中断睡眠
#define STATUS_STOPPED      4   // 停止
#define STATUS_TRACING_STOP 5   // 跟踪停止
#define STATUS_ZOMBIE       6   // 僵尸进程
#define STATUS_DEAD         7   // 死亡
#define STATUS_WAKING       8   // 唤醒中
#define STATUS_LOCKED       9   // 锁定
#define STATUS_WAITING     10   // 等待
#define STATUS_PARKED      11   // 暂停
```

## 许可证

本项目使用 BSD 许可证。详见 LICENSE 文件。

---
