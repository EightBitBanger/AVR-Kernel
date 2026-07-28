#ifndef SYSCALL_MK_H
#define SYSCALL_MK_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_mk(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    memset(path, '\0', sizeof(path));
    
    struct WorkingDirectory workingDirectory;
    kernel_get_working_directory(&workingDirectory);
    
    console_get_path(path, sizeof(path), workingDirectory.current_directory, workingDirectory.mount_directory, 256);
    strncat(path, "/", 256);
    strncat(path, args[0], 256);
    
    File file = vfs_open(path, VFS_OPEN_CREATE | VFS_OPEN_WRITE);
    if (file == VFS_INVALID_FILE) 
        return 4;
    
    if (arg_count >= 2) {
        uint32_t file_size = stoi(args[1]);
        vfs_truncate(args[0], file_size);
    }
    
    vfs_close(file);
    return 0;
}

#endif
