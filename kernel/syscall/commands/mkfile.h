#ifndef SYSCALL_MK_H
#define SYSCALL_MK_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_mk(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    File file = vfs_open(path, VFS_OPEN_CREATE | VFS_OPEN_WRITE);
    if (file == VFS_INVALID_FILE) 
        return 4;
    
    if (arg_count >= 2) {
        uint32_t file_size = stoi(args[1]);
        vfs_truncate(path, file_size); // Fixed: using full path
    }
    
    vfs_close(file);
    return 0;
}

#endif

