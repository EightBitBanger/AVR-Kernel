#ifndef SYSCALL_EXECUTE_H
#define SYSCALL_EXECUTE_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/syscall/commands/path_util.h>

int call_routine_execute(int arg_count, char** args) {
    if (arg_count < 1 || args == NULL || args[0] == NULL) {
        return -1;
    }
    
    char path[256];
    command_resolve_path(args[0], path, sizeof(path));
    
    if (!vfs_exists(path)) {
        return -1;
    }
    
    print( path );
    print(" [");
    
    for (unsigned int i=1; i < arg_count; i++) {
        
        print( args[i] );
        print(", ");
    }
    
    print("]\n");
    
    return 0;
}

#endif

