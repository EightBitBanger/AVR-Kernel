#ifndef SYSCALL_EXECUTE_H
#define SYSCALL_EXECUTE_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/scheduler/scheduler.h>

int call_routine_execute(int arg_count, char** args) {
    
    return execute_program(args[0], args, arg_count);
}

#endif
