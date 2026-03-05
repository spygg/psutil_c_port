/*
 * Example program for psutil C library
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "libpsutil/psutil.h"

int main() {
    // Initialize psutil library
    printf("Initializing psutil library...\n");
    if (psutil_init() != 0) {
        fprintf(stderr, "Failed to initialize psutil library\n");
        return 1;
    }
    printf("psutil library initialized successfully\n");
    
    // Test process creation and basic functionality
    Process* proc = process_new(0); // Current process
    if (proc == NULL) {
        fprintf(stderr, "Failed to create process object\n");
        return 1;
    }
    printf("Process object created successfully, PID: %u\n", process_get_pid(proc));
    
    // ==================== CPU Information ====================
    printf("=== CPU Information ===\n");
    
    // CPU core count
    printf("Logical CPU cores: %d\n", cpu_count(1));
    printf("Physical CPU cores: %d\n", cpu_count(0));
    
    // CPU time statistics
    psutil_cpu_times times = cpu_times(0);
    printf("CPU time stats: user=%.2f, system=%.2f, children_user=%.2f, children_system=%.2f\n", 
           times.user, times.system, times.children_user, times.children_system);
    
    // CPU usage (sample every second for 3 seconds)
    printf("CPU usage (per second sampling):\n");
    for (int i = 0; i < 3; i++) {
        printf("  %.2f%%\n", cpu_percent(1.0, 0));
    }
    
    // CPU statistics
    psutil_cpu_stats cpu_stats_info = cpu_stats();
    printf("CPU stats: ctx_switches=%d, interrupts=%d, soft_interrupts=%d, syscalls=%d\n", 
           cpu_stats_info.ctx_switches, cpu_stats_info.interrupts, 
           cpu_stats_info.soft_interrupts, cpu_stats_info.syscalls);
    
    // ==================== Memory Information ====================
    printf("\n=== Memory Information ===\n");
    
    // Virtual memory
    psutil_memory_info vmem = virtual_memory();
    printf("Virtual memory: total=%.2f GB, available=%.2f GB, usage=%.2f%%\n", 
           vmem.vms / (1024.0 * 1024.0 * 1024.0), 
           (vmem.vms - vmem.rss) / (1024.0 * 1024.0 * 1024.0), 
           (double)vmem.rss / vmem.vms * 100.0);
    
    // Swap memory
    psutil_memory_info smem = swap_memory();
    printf("Swap memory: total=%.2f GB, used=%.2f GB, usage=%.2f%%\n", 
           smem.vms / (1024.0 * 1024.0 * 1024.0), 
           smem.rss / (1024.0 * 1024.0 * 1024.0), 
           (double)smem.rss / smem.vms * 100.0);
    
    // ==================== Disk Information ====================
    printf("\n=== Disk Information ===\n");
    
    // Disk partitions
    psutil_disk_partition* partitions = disk_partitions(1);
    if (partitions != NULL) {
        int i = 0;
        while (partitions[i].device[0] != '\0') {
            printf("Partition: device=%s, mountpoint=%s, fstype=%s\n", 
                   partitions[i].device, partitions[i].mountpoint, partitions[i].fstype);
            
            // Partition usage
            psutil_disk_usage usage = disk_usage(partitions[i].mountpoint);
            printf("  Usage: total=%.2f GB, used=%.2f GB, free=%.2f GB, usage=%.2f%%\n", 
                   usage.total / (1024.0 * 1024.0 * 1024.0), 
                   usage.used / (1024.0 * 1024.0 * 1024.0), 
                   usage.free / (1024.0 * 1024.0 * 1024.0), 
                   usage.percent);
            i++;
        }
        free(partitions);
    }
    
    // Disk I/O statistics
    psutil_io_counters disk_io = disk_io_counters(0);
    printf("Disk I/O: read=%.2f GB, write=%.2f GB, read_count=%llu, write_count=%llu\n", 
           disk_io.read_bytes / (1024.0 * 1024.0 * 1024.0), 
           disk_io.write_bytes / (1024.0 * 1024.0 * 1024.0), 
           disk_io.read_count, disk_io.write_count);
    
    // ==================== Network Information ====================
    printf("\n=== Network Information ===\n");
    
    // Network I/O statistics
    psutil_io_counters net_io = net_io_counters(0);
    printf("Network I/O: sent=%.2f MB, received=%.2f MB\n", 
           net_io.write_bytes / (1024.0 * 1024.0), 
           net_io.read_bytes / (1024.0 * 1024.0));
    
    // Network interface addresses
    int count = 0;
    psutil_net_if_addr* addrs = net_if_addrs(&count);
    if (addrs != NULL && count > 0) {
        printf("Network interface addresses:\n");
        for (int i = 0; i < count; i++) {
            printf("  Interface: %s, Address: %s, Netmask: %s, Broadcast: %s\n", 
                   addrs[i].name, addrs[i].address, addrs[i].netmask, addrs[i].broadcast);
        }
        free(addrs);
    }
    
    // Network interface status
    psutil_net_if_stat* stats = net_if_stats(&count);
    if (stats != NULL && count > 0) {
        printf("Network interface status:\n");
        for (int i = 0; i < count; i++) {
            printf("  Interface: %s, IsUp=%d, Speed=%d Mbps, MTU=%d\n", 
                   stats[i].name, stats[i].isup, stats[i].speed, stats[i].mtu);
        }
        free(stats);
    }
    
    // ==================== Process Information ====================
    printf("\n=== Process Information ===\n");
    
    // Get all process PIDs
    int pid_count = 0;
    uint32_t* pids_list = pids(&pid_count);
    printf("Total running processes: %d\n", pid_count);
    if (pids_list != NULL) {
        free(pids_list);
    }
    
    // Current process details
    uint32_t pid = process_get_pid(proc);
    printf("\nCurrent process (PID=%u):\n", pid);
    printf("  Name: %s\n", process_get_name(proc));
    printf("  Executable: %s\n", process_get_exe(proc));
    printf("  Username: %s\n", process_get_username(proc));
    double create_time = process_get_create_time(proc);
    if (create_time > 0.0) {
        time_t t = (time_t)create_time;
        printf("  Create time: %s", ctime(&t));
    } else {
        printf("  Create time: (null)\n");
    }
    printf("  Current working directory: %s\n", process_get_cwd(proc));
    printf("  Nice value: %d\n", process_get_nice(proc));
    printf("  Thread count: %d\n", process_get_num_threads(proc));
    
    // Process memory information
    psutil_memory_info mem_info = process_get_memory_info(proc);
    printf("  Memory info: RSS=%llu bytes, VMS=%llu bytes\n", 
           mem_info.rss, mem_info.vms);
    
    // Process CPU time
    psutil_cpu_times proc_times = process_get_cpu_times(proc);
    printf("  CPU time: user=%.2f, system=%.2f\n", 
           proc_times.user, proc_times.system);
    
    // ==================== Other System Information ====================
    printf("\n=== Other System Information ===\n");
    
    // Logged in users
    int user_count = 0;
    psutil_user* users_list = users(&user_count);
    if (users_list != NULL && user_count > 0) {
        printf("Logged in users:\n");
        for (int i = 0; i < user_count; i++) {
            double start_time = users_list[i].started;
            printf("  User: %s, Terminal: %s, Login time: %s", 
                   users_list[i].name, users_list[i].terminal, 
                   ctime((time_t*)&start_time));
        }
        free(users_list);
    }
    
    // System boot time
    double boot_time_val = boot_time();
    if (boot_time_val > 0.0) {
        time_t boot_time_t = (time_t)boot_time_val;
        printf("System boot time: %s", ctime(&boot_time_t));
        printf("System uptime: %.0f seconds\n", time(NULL) - boot_time_val);
    } else {
        printf("System boot time: (null)\n");
        printf("System uptime: 0 seconds\n");
    }
    
    // Free resources
    process_free(proc);
    
    return 0;
}
