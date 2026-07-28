#ifndef SYSCALL_CLEAR_H
#define SYSCALL_CLEAR_H

#include <stdint.h>

#include <kernel/kernel.h>

int call_routine_clear(int arg_count, char** args) {
    display_clear();
    display_cursor_set_line(0);
    display_cursor_set_position(0);
    return 0;
}

#endif
