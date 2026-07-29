#ifndef SYSCALL_CD_H
#define SYSCALL_CD_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>

#include <kernel/kernel.h>
#include <kernel/knode.h>
#include <kernel/memory/malloc.h>
#include <kernel/fs/fs.h>
#include <kernel/console/console_const.h>

int call_routine_chdir(int arg_count, char** args) {
    if (arg_count < 1 || args[0] == NULL || args[0][0] == '\0') 
        return -1; // No path provided
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    // If the command is strictly "cd .", retrieve and print the current path
    if (strcmp(args[0], ".") == 0) {
        char path_buf[256];
        console_get_path(path_buf, sizeof(path_buf) - 2, fs_current.current_directory, fs_current.mount_directory, 256);
        print(path_buf);
        print("\n");
        return 0;
    }
    
    int is_absolute = (args[0][0] == '/');
    if (is_absolute) {
        fs_current.current_directory = knode_get_root();
        fs_current.mount_directory   = FS_NULL;
        fs_current.mount_device      = FS_NULL;
        fs_current.mount_root        = FS_NULL;
    }
    
    char path_copy[256]; 
    strncpy(path_copy, args[0], sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    cstr_tok_t tok;
    cstr_tok_init(&tok, path_copy, "/");
    
    char* dirname = cstr_tok_next(&tok);
    
    while (dirname != NULL) {
        
        // ==========================================
        // MOUNTED DIRECTORY RESOLUTION (Inside File System)
        // ==========================================
        if (fs_current.mount_device != FS_NULL && fs_current.mount_device != KMALLOC_NULL) {
            
            // Retrieve active device context directly from mount point knode reference 1
            struct FSDeviceContext* ctx = (struct FSDeviceContext*)knode_get_reference(fs_current.current_directory, 1);
            if (!ctx) {
                print(msg_dir_invalid);
                return -1;
            }

            if (strcmp(dirname, ".") == 0) {
                dirname = cstr_tok_next(&tok);
                continue;
            }
            
            if (strcmp(dirname, "..") == 0) {
                // Stepping out of mount point back into virtual file system knodes
                if (fs_current.mount_directory == fs_current.mount_root) {
                    fs_current.mount_directory   = FS_NULL;
                    fs_current.mount_device      = FS_NULL;
                    fs_current.mount_root        = FS_NULL;
                    fs_current.current_directory = knode_get_parent(fs_current.current_directory);
                } else {
                    uint32_t parent_dir = fs_directory_get_parent(ctx, fs_current.mount_directory);
                    if (parent_dir == FS_NULL || parent_dir == 0) {
                        fs_current.mount_directory = fs_current.mount_root;
                    } else {
                        fs_current.mount_directory = parent_dir;
                    }
                }
                dirname = cstr_tok_next(&tok);
                continue;
            }
            
            uint32_t reference = fs_directory_find(ctx, fs_current.mount_directory, dirname);
            if (reference == FS_NULL || !fs_check_directory_valid(ctx, reference)) {
                print(msg_dir_invalid);
                return -1;
            }
            
            fs_current.mount_directory = reference;
            dirname = cstr_tok_next(&tok);
            continue;
        }
        
        // ==========================================
        // VIRTUAL FILE SYSTEM RESOLUTION (KNode)
        // ==========================================
        if (strcmp(dirname, ".") == 0) {
            dirname = cstr_tok_next(&tok);
            continue;
        }
        
        uint32_t target_directory;
        if (strcmp(dirname, "..") == 0) {
            target_directory = knode_get_parent(fs_current.current_directory);
        } else {
            target_directory = knode_find_by_name(fs_current.current_directory, dirname);
        }
        
        if (target_directory == KMALLOC_NULL || target_directory == 0 || target_directory == KNODE_NULL) {
            print(msg_dir_invalid);
            return -1;
        }
        
        uint8_t flags = kmalloc_get_flags(target_directory);
        
        // Check if target directory is a mount point
        if (flags & KMALLOC_FLAG_MOUNT) {
            uint32_t reference_address = knode_get_reference(target_directory, 0);
            struct FSDeviceContext* ctx = (struct FSDeviceContext*)knode_get_reference(target_directory, 1);
            
            if (!ctx) {
                print(msg_dir_invalid);
                return -1;
            }

            struct FSPartitionBlock header;
            fs_device_get_partition(ctx, &header);
            
            fs_current.current_directory = target_directory;
            fs_current.mount_device      = reference_address;
            fs_current.mount_directory   = header.root_directory;
            fs_current.mount_root        = header.root_directory;
            
            dirname = cstr_tok_next(&tok);
            continue;
        }
        
        if ((flags & KMALLOC_FLAG_DIRECTORY) == 0) {
            print(msg_dir_invalid);
            return -1;
        }
        
        fs_current.current_directory = target_directory;
        dirname = cstr_tok_next(&tok);
    }
    
    kernel_set_working_directory(&fs_current);
    
    // Update console command prompt string
    char path_buf[256]; 
    console_get_path(path_buf, sizeof(path_buf) - 2, fs_current.current_directory, fs_current.mount_directory, 256);
    
    size_t len = strlen(path_buf);
    path_buf[len]     = '>';
    path_buf[len + 1] = '\0';
    
    console_prompt_set_string(path_buf);
    
    return 0;
}

#endif
