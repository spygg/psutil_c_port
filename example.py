import psutil
import os
import time

# ==================== CPU 信息 ====================
print("=== CPU 信息 ===")

# CPU 核心数
print(f"逻辑 CPU 核心数: {psutil.cpu_count()}")
print(f"物理 CPU 核心数: {psutil.cpu_count(logical=False)}")

# CPU 时间统计 (用户、系统、空闲等)
cpu_times = psutil.cpu_times()
print(f"CPU 时间统计: {cpu_times}")

# CPU 使用率 (每秒采样一次，持续3秒)
print("CPU 使用率 (每秒采样):")
for i in range(3):
    print(f"  {psutil.cpu_percent(interval=1)}%")

# 每个 CPU 核心的使用率
print("每个 CPU 核心使用率:")
for i, percent in enumerate(psutil.cpu_percent(interval=1, percpu=True)):
    print(f"  核心 {i}: {percent}%")

# CPU 频率
cpu_freq = psutil.cpu_freq()
if cpu_freq:
    print(f"CPU 频率: 当前={cpu_freq.current:.2f} MHz, 最小={cpu_freq.min:.2f} MHz, 最大={cpu_freq.max:.2f} MHz")

# CPU 统计 (上下文切换、中断等)
cpu_stats = psutil.cpu_stats()
print(f"CPU 统计: 上下文切换={cpu_stats.ctx_switches}, 中断={cpu_stats.interrupts}, 软中断={cpu_stats.soft_interrupts}, 系统调用={cpu_stats.syscalls}")

# ==================== 内存信息 ====================
print("\n=== 内存信息 ===")

# 虚拟内存
mem = psutil.virtual_memory()
print(f"虚拟内存: 总计={mem.total / (1024**3):.2f} GB, 可用={mem.available / (1024**3):.2f} GB, 使用率={mem.percent}%")

# 交换内存
swap = psutil.swap_memory()
print(f"交换内存: 总计={swap.total / (1024**3):.2f} GB, 已用={swap.used / (1024**3):.2f} GB, 使用率={swap.percent}%")

# ==================== 磁盘信息 ====================
print("\n=== 磁盘信息 ===")

try:
    # 磁盘分区
    partitions = psutil.disk_partitions()
    for part in partitions:
        print(f"分区: 设备={part.device}, 挂载点={part.mountpoint}, 文件系统={part.fstype}")
        try:
            usage = psutil.disk_usage(part.mountpoint)
            print(f"  使用情况: 总计={usage.total / (1024**3):.2f} GB, 已用={usage.used / (1024**3):.2f} GB, 空闲={usage.free / (1024**3):.2f} GB, 使用率={usage.percent}%")
        except PermissionError:
            print("  无权限访问此分区")
except:
    pass

# 磁盘 I/O 统计
disk_io = psutil.disk_io_counters()
if disk_io:
    print(f"磁盘 I/O: 读取={disk_io.read_bytes / (1024**3):.2f} GB, 写入={disk_io.write_bytes / (1024**3):.2f} GB, 读次数={disk_io.read_count}, 写次数={disk_io.write_count}")

# ==================== 网络信息 ====================
print("\n=== 网络信息 ===")

# 网络 I/O 统计
net_io = psutil.net_io_counters()
print(f"网络 I/O: 发送={net_io.bytes_sent / (1024**2):.2f} MB, 接收={net_io.bytes_recv / (1024**2):.2f} MB")

# 网络连接 (可能需要 root/管理员权限)
print("当前网络连接 (前5个):")
try:
    connections = psutil.net_connections()
    for conn in connections[:5]:
        print(f"  {conn}")
except (psutil.AccessDenied, PermissionError):
    print("  无权限查看所有网络连接")

# 网络接口地址
addrs = psutil.net_if_addrs()
for iface, addr_list in addrs.items():
    print(f"接口: {iface}")
    for addr in addr_list:
        print(f"  地址类型: {addr.family}, 地址: {addr.address}, 掩码: {addr.netmask}, 广播: {addr.broadcast}")

# 网络接口状态
stats = psutil.net_if_stats()
for iface, stat in stats.items():
    print(f"接口 {iface}: 运行={stat.isup}, 速度={stat.speed} Mbps, MTU={stat.mtu}")

# ==================== 传感器信息 (仅部分平台支持) ====================
print("\n=== 传感器信息 ===")


# 电池
battery = psutil.sensors_battery()
if battery:
    print(f"电池: 电量={battery.percent}%, 充电中={battery.power_plugged}, 剩余时间={battery.secsleft if battery.secsleft != psutil.POWER_TIME_UNLIMITED else '无限'}秒")

# ==================== 进程管理 ====================
print("\n=== 进程信息 ===")

# 获取所有进程 PID
print(f"当前运行的进程总数: {len(psutil.pids())}")

# 迭代所有进程并显示部分信息 (只显示前5个)
print("前5个进程信息:")
for proc in psutil.process_iter(['pid', 'name', 'cpu_percent', 'memory_percent', 'create_time']):
    try:
        info = proc.info
        print(f"  PID={info['pid']}, 名称={info['name']}, CPU%={info['cpu_percent']:.1f}, 内存%={info['memory_percent']:.1f}, 创建时间={time.ctime(info['create_time'])}")
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass
    # 只显示前5个
    if proc.pid == list(psutil.process_iter())[4].pid:
        break

# 通过 PID 获取单个进程
pid = os.getpid()  # 当前脚本的 PID
current_process = psutil.Process(pid)
print(f"\n当前脚本进程 (PID={pid}):")
print(f"  名称: {current_process.name()}")
print(f"  可执行路径: {current_process.exe()}")
print(f"  命令行: {' '.join(current_process.cmdline())}")
print(f"  状态: {current_process.status()}")
print(f"  创建时间: {time.ctime(current_process.create_time())}")
print(f"  CPU 使用率: {current_process.cpu_percent(interval=0.1)}%")
print(f"  内存信息: {current_process.memory_info()}")
print(f"  打开的文件: {len(current_process.open_files())}")
print(f"  网络连接: {len(current_process.connections())}")

# 进程子进程
children = current_process.children()
print(f"  子进程数: {len(children)}")

# 终止进程示例 (谨慎使用)
# 如果需要结束进程，可以调用 proc.terminate() 或 proc.kill()
# 示例: 查找并结束一个特定进程 (这里不执行，仅展示)
# for proc in psutil.process_iter(['name']):
#     if proc.info['name'] == 'notepad.exe':
#         proc.terminate()
#         proc.wait(timeout=3)

# ==================== 其他系统信息 ====================
print("\n=== 其他系统信息 ===")

# 登录用户
users = psutil.users()
for user in users:
    print(f"用户: {user.name}, 终端: {user.terminal}, 主机: {user.host}, 登录时间: {time.ctime(user.started)}")

# 系统启动时间
boot_time = psutil.boot_time()
print(f"系统启动时间: {time.ctime(boot_time)}")
print(f"系统已运行: {time.time() - boot_time:.0f} 秒")
