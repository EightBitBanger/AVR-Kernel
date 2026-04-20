#ifndef SYSCALL_RM_H
#define SYSCALL_RM_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_rm(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device == FS_NULL) 
        return 2;
    
    struct FSPartitionBlock partition;
    if (fs_device_open(fs_current.mount_device, &partition) == FS_NULL) 
        return 3;
    
    uint32_t address = fs_directory_find(fs_current.mount_directory, args[0]);
    if (address == FS_NULL) 
        return 4;
    
    uint8_t attributes;
    fs_file_get_attributes(address, &attributes);
    
    if (attributes & FS_ATTRIBUTE_DIRECTORY) {
        uint32_t reference_count = fs_directory_get_reference_count(address);
        if (reference_count != 0) 
            return 5;
    }
    
    fs_directory_remove_reference(fs_current.mount_directory, address);
    fs_free(address);
    fs_bitmap_flush();
    
    return 0;
}

#endif
