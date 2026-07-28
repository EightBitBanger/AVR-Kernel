#ifndef SYSCALL_RM_H
#define SYSCALL_RM_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_rm(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    memset(path, '\0', sizeof(path));
    
    struct WorkingDirectory workingDirectory;
    kernel_get_working_directory(&workingDirectory);
    
    console_get_path(path, sizeof(path), workingDirectory.current_directory, workingDirectory.mount_directory, 256);
    strncat(path, "/", 256);
    strncat(path, args[0], 256);
    
    if (!vfs_exists(path)) 
        return 4;
    
    if (!vfs_remove(path)) 
        return 5;
    
    return 0;
}

#endif
