#ifndef SYSCALL_MK_H
#define SYSCALL_MK_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_mk(int arg_count, char** args) {
    
    // TODO convert to VFS functions
    
    if (arg_count == 0) 
        return 1;
    
    /*
    for (unsigned int i=0; i < arg_count; i++) {
        print(args[i]);
        print("\n");
    }
    return 0;
    */
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    if (fs_current.mount_device == FS_NULL) 
        return 2;
    
    struct FSPartitionBlock partition;
    
    struct FSDeviceContext device_context = fs_device_open(fs_current.mount_device, &partition, FS_DEVICE_TYPE_ATA);
    if (device_context.is_open == false) 
        return 3;
    
    uint32_t file_size = 0;
    if (arg_count == 2) file_size = stoi(args[1]);
    
    uint32_t file_address = fs_file_create(args[0], FS_PERMISSION_READ | FS_PERMISSION_WRITE, file_size, fs_current.mount_directory);
    if (file_address == FS_NULL) 
        return 4;
    
    fs_bitmap_flush();
    return 0;
}

#endif
