#ifndef SYSCALL_RN_H
#define SYSCALL_RN_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>

int call_routine_rename(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device == FS_NULL) 
        return 2;
    
    struct FSPartitionBlock partition;
    if (fs_device_open(fs_current.mount_device, &partition) == FS_NULL) 
        return 3;
    
    uint32_t target_address = fs_directory_find(fs_current.mount_directory, args[0]);
    if (target_address == FS_NULL) 
        return 4;
    
    fs_file_set_name(target_address, args[1]);
    
    fs_bitmap_flush();
    return 0;
}

#endif
