#ifndef SYSCALL_TYPE_H
#define SYSCALL_TYPE_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_type(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
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

