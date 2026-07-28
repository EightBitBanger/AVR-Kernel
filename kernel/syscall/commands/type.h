#ifndef SYSCALL_TYPE_H
#define SYSCALL_TYPE_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_type(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    memset(path, '\0', sizeof(path));
    
    struct WorkingDirectory workingDirectory;
    kernel_get_working_directory(&workingDirectory);
    
    console_get_path(path, sizeof(path), workingDirectory.current_directory, workingDirectory.mount_directory, 256);
    strncat(path, "/", 256);
    strncat(path, args[0], 256);
    
    File file = vfs_open(path, VFS_OPEN_READ);
    if (file == VFS_INVALID_FILE) 
        return 2;
    
    uint32_t file_size = vfs_get_size(file);
    for (uint32_t i = 0; i < file_size; i++) {
        char ch[2] = {0, '\0'};
        if (vfs_read(file, &ch[0], 1) <= 0) 
            break;
        
        if (ch[0] == '\0') 
            break;
        
        print(ch);
    }
    
    vfs_close(file);
    return 0;
}

#endif
