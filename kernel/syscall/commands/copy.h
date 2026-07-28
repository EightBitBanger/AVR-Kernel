#ifndef SYSCALL_COPY_H
#define SYSCALL_COPY_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_copy(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    char path[256];
    memset(path, '\0', sizeof(path));
    
    struct WorkingDirectory workingDirectory;
    kernel_get_working_directory(&workingDirectory);
    
    console_get_path(path, sizeof(path), workingDirectory.current_directory, workingDirectory.mount_directory, 256);
    strncat(path, "/", 256);
    strncat(path, args[0], 256);
    
    if (!vfs_exists(path)) 
        return 3; // Source does not exist
    
    if (vfs_is_directory(path)) 
        return 4; // Source is a directory
    
    char dest_path[256];
    strncpy(dest_path, args[1], sizeof(dest_path) - 1);
    dest_path[sizeof(dest_path) - 1] = '\0';
    
    if (vfs_exists(args[1])) {
        if (vfs_is_directory(args[1])) {
            const char* filename = strrchr(path, '/');
            if (filename == NULL) {
                filename = path;
            } else {
                filename++; // Skip '/'
            }
            
            size_t dest_len = strlen(dest_path);
            if (dest_len > 0 && dest_path[dest_len - 1] != '/') {
                strcat(dest_path, "/");
            }
            strcat(dest_path, filename);
            
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
