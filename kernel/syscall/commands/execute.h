#ifndef SYSCALL_EXECUTE_H
#define SYSCALL_EXECUTE_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/scheduler/scheduler.h>

int call_routine_execute(int arg_count, char** args) {
    if (!vfs_exists(args[0])) {
        return -1;
    }
    
    print( args[0] );
    print(" [");
    
    for (unsigned int i=1; i < arg_count; i++) {
        
        print( args[i] );
        print(", ");
    }
    
    print("]\n");
    
    return 0;
}

#endif
