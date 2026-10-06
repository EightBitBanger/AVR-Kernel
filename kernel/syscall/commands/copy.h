#ifndef SYSCALL_COPY_H
#define SYSCALL_COPY_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_copy(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    if (!vfs_exists(path)) 
        return 3; // Source does not exist
    
    if (vfs_directory_check(path)) 
        return 4; // Source is a directory
    
    char dest_path[256];
    command_resolve_path(args[1], dest_path, sizeof(dest_path));
    
    if (vfs_exists(dest_path)) {
        if (vfs_directory_check(dest_path)) {
            const char* filename = strrchr(path, '/');
            if (filename == NULL) {
                filename = path;
            } else {
                filename++; // Skip '/'
            }
            
            size_t dest_len = strlen(dest_path);
            if (dest_len > 0 && dest_path[dest_len - 1] != '/') {
                strncat(dest_path, "/", sizeof(dest_path));
            }
            if (strlen(dest_path) + strlen(filename) >= sizeof(dest_path)) {
                return 7; // Destination path too long
            }
            strncat(dest_path, filename, sizeof(dest_path));
            
            if (vfs_exists(dest_path)) {
                return 6; // File already exists in destination directory
            }
        } else {
            return 5; // Destination file already exists
        }
    }
    
    File src_file = vfs_open(path, VFS_OPEN_READ);
    if (src_file == VFS_INVALID_FILE) 
        return 3;
    
    File dst_file = vfs_open(dest_path, VFS_OPEN_CREATE | VFS_OPEN_WRITE);
    if (dst_file == VFS_INVALID_FILE) {
        vfs_close(src_file);
        return 3;
    }
    
    uint32_t source_size = vfs_get_size(src_file);
    for (uint32_t i = 0; i < source_size; i++) {
        uint8_t byte;
        if (vfs_read(src_file, &byte, 1) <= 0) 
            break;
        vfs_write(dst_file, &byte, 1);
    }
    
    vfs_close(src_file);
    vfs_close(dst_file);
    
    // Preserve source permissions onto destination file
    uint8_t perm;
    if (vfs_get_permissions(path, &perm)) {
        vfs_set_permissions(dest_path, perm);
    }
    
    return 0;
}

#endif

