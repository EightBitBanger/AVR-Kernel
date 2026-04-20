#ifndef SYSCALL_MKDIR_H
#define SYSCALL_MKDIR_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_mkdir(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device == FS_NULL) 
        return 2;
    
    struct FSPartitionBlock partition;
    if (fs_device_open(fs_current.mount_device, &partition) == FS_NULL) 
        return 3;
    
    uint32_t directory_address = fs_directory_create(args[0], FS_PERMISSION_READ | FS_PERMISSION_WRITE, fs_current.mount_directory);
    if (directory_address == FS_NULL) 
        return 4;
    
    fs_bitmap_flush();
    return 0;
}

#endif
