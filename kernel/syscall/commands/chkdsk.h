#ifndef SYSCALL_CHKDSK_H
#define SYSCALL_CHKDSK_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_chkdsk(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    if (fs_current.mount_device == FS_NULL) {
        print("Device unmounted\n");
        return 1;
    }
    
    // Use the mount's cached context (also picks up the right device type)
    struct FSDeviceContext* ctx =
        (struct FSDeviceContext*)knode_get_reference(fs_current.current_directory, 1);
    if (ctx == NULL || (uint32_t)ctx == KMALLOC_NULL) {
        print("Device unmounted\n");
        return 1;
    }
    
    struct FSPartitionBlock partition;
    if (fs_device_get_partition(ctx, &partition) != 0 || partition.sector_size == 0) {
        print("Invalid partition\n");
        return 1;
    }
    
    print_int(partition.sector_size);
    print("B per sector\n");
    
    print_int(partition.total_size / partition.sector_size);
    print(" sectors\n");
    
    print_int(partition.total_size);
    print("B total\n\n");
    
    //uint32_t root_directory = partition.root_directory;
    return 0;
}

#endif

