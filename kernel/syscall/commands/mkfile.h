#ifndef SYSCALL_MK_H
#define SYSCALL_MK_H

#include <stdint.h>
#include <kernel/kernel.h>

int call_routine_mk(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device == FS_NULL) 
        return 2;
    
    struct FSPartitionBlock partition;
    if (fs_device_open(fs_current.mount_device, &partition) == FS_NULL) 
        return 3;
    
    uint32_t file_size = 0;
    if (arg_count == 2) file_size = strtol(args[1], NULL, 10);
    
    uint32_t file_address = fs_file_create(args[0], FS_PERMISSION_READ | FS_PERMISSION_WRITE, file_size, fs_current.mount_directory);
    if (file_address == FS_NULL) 
        return 4;
    
    fs_bitmap_flush();
    return 0;
}

#endif
