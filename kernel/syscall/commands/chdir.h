#ifndef SYSCALL_CD_H
#define SYSCALL_CD_H

#include <stdint.h>
#include <kernel/util/string.h>

#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_chdir(int arg_count, char** args) {
    if (arg_count < 1 || args[0] == NULL || args[0][0] == '\0') 
        return -1; // No path provided
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    int is_absolute = (args[0][0] == '/');
    if (is_absolute) {
        fs_current.current_directory = knode_get_root();
        fs_current.mount_directory = FS_NULL;
        fs_current.mount_device = FS_NULL;
        fs_current.mount_root = FS_NULL;
    }
    
    char path_copy[256]; 
    strncpy(path_copy, args[0], sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* dirname = strtok(path_copy, "/");
    
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
            
            struct FSPartitionBlock header;
            fs_device_open(reference_address, &header);
            
            fs_current.current_directory = target_directory;
            fs_current.mount_device      = reference_address;
            fs_current.mount_directory   = header.root_directory;
            fs_current.mount_root        = header.root_directory;
            
            dirname = strtok(NULL, "/");
            continue;
        }
        
        if ((flags & KMALLOC_FLAG_DIRECTORY) == 0) 
            break;
        
        fs_current.current_directory = target_directory;
        dirname = strtok(NULL, "/");
    }
    
    kernel_set_working_directory(&fs_current);
    
    char path_buf[128]; 
    console_get_path(path_buf, sizeof(path_buf) - 2, fs_current.current_directory, fs_current.mount_directory, 2);
    
    size_t len = strlen(path_buf);
    path_buf[len] = '>';
    path_buf[len + 1] = '\0';
    
    console_prompt_set_string(path_buf);
    return 0;
}

#endif
