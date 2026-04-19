#ifndef SYSCALL_CHKDSK_H
#define SYSCALL_CHKDSK_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_chkdsk(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device == FS_NULL) {
        print("Device unmounted\n");
        return 1;
    }
    
    struct FSPartitionBlock partition;
    fs_device_open(fs_current.mount_device, &partition);
    
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
