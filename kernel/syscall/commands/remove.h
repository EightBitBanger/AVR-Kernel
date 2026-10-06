#ifndef SYSCALL_RM_H
#define SYSCALL_RM_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_rm(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    if (!vfs_exists(path)) 
        return 4;
    
    if (!vfs_remove(path)) 
        return 5;
    
    return 0;
}

#endif

