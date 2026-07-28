#ifndef SYSCALL_RN_H
#define SYSCALL_RN_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_rename(int arg_count, char** args) {
    if (arg_count < 2) 
        return 1;
    
    if (!vfs_exists(args[0])) 
        return 4;
    
    if (!vfs_rename(args[0], args[1])) 
        return 2;
    
    return 0;
}

#endif
