#ifndef SYSCALL_CD_H
#define SYSCALL_CD_H

#include <stdint.h>
#include <string.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_chdir(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    int is_absolute = (args[0][0] == '/');
    
    if (is_absolute) {
        fs_current.current_directory = knode_get_root();
        fs_current.mount_directory = FS_NULL;
        fs_current.mount_device = FS_NULL;
        fs_current.mount_root = FS_NULL;
    }
    
    char* dirname = strtok(args[0], "/");
    
    while (dirname != NULL) {
        if (fs_current.mount_device != KMALLOC_NULL) {
            
            if (strcmp(dirname, "..") == 0) {
                if (fs_current.mount_directory == fs_current.mount_root) {
                    fs_current.mount_directory = FS_NULL;
                    fs_current.mount_device = FS_NULL;
                    fs_current.mount_root = FS_NULL;
                    fs_current.current_directory = knode_get_parent(fs_current.current_directory);
                } else {
                    fs_current.mount_directory = fs_directory_get_parent(fs_current.mount_directory);
                    if (fs_current.mount_directory == FS_NULL)
                        fs_current.mount_directory = fs_current.mount_root;
                }
                
                dirname = strtok(NULL, "/");
                continue;
            }
            
            uint32_t reference = fs_directory_find(fs_current.mount_directory, dirname);
            if (reference == FS_NULL) 
                break;
            
            fs_current.mount_directory = reference;
            dirname = strtok(NULL, "/");
            continue;
        }
        
        uint32_t target_directory;
        
        if (strcmp(dirname, "..") == 0) {
            target_directory = knode_get_parent(fs_current.current_directory);
        } else {
            target_directory = knode_find_by_name(fs_current.current_directory, dirname);
        }
        
        if (target_directory == KMALLOC_NULL) 
            break;
        
        uint8_t flags = kmalloc_get_flags(target_directory);
        
        if (flags & KMALLOC_FLAG_MOUNT) {
            uint32_t reference_address = knode_get_reference(target_directory, 0);
            
            uint8_t bitmap[256];
            uint8_t dirty[256];
            struct FSPartitionBlock header;
            fs_device_open(reference_address, &header);
            
            fs_current.current_directory = target_directory;
            fs_current.mount_device      = reference_address;
            fs_current.mount_directory   = header.root_directory;
            fs_current.mount_root        = header.root_directory;
            
            char path_buf[32];
            console_get_path(path_buf, 32, target_directory, fs_current.mount_directory, 2);
            
            strcpy(&path_buf[strlen(path_buf)], ">\0");
            console_set_prompt(path_buf);
            
            dirname = strtok(NULL, "/");
            continue;
        }
        
        if ((flags & KMALLOC_FLAG_DIRECTORY) == 0) 
            break;
        
        fs_current.current_directory = target_directory;
        
        dirname = strtok(NULL, "/");
    }
    
    kernel_set_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    char path_buf[32];
    console_get_path(path_buf, 32, fs_current.current_directory, fs_current.mount_directory, 2);
    
    strcpy(&path_buf[strlen(path_buf)], ">\0");
    console_set_prompt(path_buf);
    return 0;
}

#endif
