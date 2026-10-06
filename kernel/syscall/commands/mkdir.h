#ifndef SYSCALL_MKDIR_H
#define SYSCALL_MKDIR_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_mkdir(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    if (!vfs_mkdir(path)) 
        return 4;
    
    return 0;
}

#endif

