#ifndef SYSCALL_COPY_H
#define SYSCALL_COPY_H

#include <stdint.h>
#include <kernel/kernel.h>

int call_routine_copy(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_directory == FS_NULL) 
        return 2;
    
    uint32_t source_address = fs_directory_find(fs_current.mount_directory, args[0]);
    uint32_t destination_address = fs_directory_find(fs_current.mount_directory, args[1]);
    if (source_address == FS_NULL) 
        return 3; // Source does not exist
    
    if (fs_is_directory(source_address)) 
        return 4; // Source is a directory
    
    uint32_t source_size = fs_file_get_size(source_address);
    
    // Copy to a directory
    if (destination_address != FS_NULL) {
        if (fs_is_directory(destination_address)) {
            uint32_t check_address = fs_directory_find(destination_address, args[0]);
            if (check_address != FS_NULL) 
                return 6; // File exists in destination directory
            
            // Create file in destination directory
            
            destination_address = fs_file_create(args[0], FS_PERMISSION_READ | FS_PERMISSION_WRITE, source_size, destination_address);
            
        } else {
            return 5; // File already exists
        }
    } else {
        destination_address = fs_file_create(args[1], FS_PERMISSION_READ | FS_PERMISSION_WRITE, source_size, fs_current.mount_directory);
    }
    
    if (destination_address == FS_NULL) 
        return 3;
    
    // Copy to the new file
    uint8_t perm;
    fs_file_get_permissions(source_address, &perm);
    fs_file_set_permissions(destination_address, perm);
    
    FileHandle source;
    FileHandle destination;
    
    if (fs_file_open(&source, source_address, FS_FILE_MODE_READ) == true && 
        fs_file_open(&destination, destination_address, FS_FILE_MODE_WRITE) == true) {
        
        for (uint32_t i=0; i < source_size; i++) {
            uint8_t byte;
            
            fs_file_read(&source, &byte, 1);
            fs_file_write(&destination, &byte, 1);
        }
    } else {
        fs_bitmap_flush();
        return 4;
    }
    
    if (source.is_open) fs_file_close(&source);
    if (destination.is_open) fs_file_close(&destination);
    
    fs_bitmap_flush();
    return 0;
}

#endif
