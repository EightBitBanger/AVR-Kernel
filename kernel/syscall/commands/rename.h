#ifndef SYSCALL_RN_H
#define SYSCALL_RN_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_rename(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    if (!vfs_exists(path)) 
        return 4;
    
    // args[1] is the new NAME (not a path)
    if (!vfs_rename(path, args[1])) 
        return 2;
    
    return 0;
}

#endif

