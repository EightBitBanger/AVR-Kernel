#include <kernel/boot/avr/heap.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/syscall.h>
#include <kernel/syscall/console.h>
#include <kernel/syscall/print.h>

#include <kernel/fs/fs.h>
#include <kernel/emulation/x4/x4.h>
#include <kernel/syscall/graphix.h>


#include <kernel/syscall/commands/execute.h>
#include <kernel/syscall/commands/ls.h>
#include <kernel/syscall/commands/cd.h>

#include <kernel/syscall/commands/mk.h>
#include <kernel/syscall/commands/mkdir.h>
#include <kernel/syscall/commands/rm.h>
#include <kernel/syscall/commands/copy.h>

#include <kernel/syscall/commands/boot.h>
#include <kernel/syscall/commands/format.h>
#include <kernel/syscall/commands/chkdsk.h>

#include <kernel/syscall/commands/graph.h>

#include <kernel/syscall/commands/type.h>


// Syscall command table

struct CommandFunction {const char* name; uint16_t id; CommandFunction function; uint8_t flags;};
static const struct CommandFunction command_table[] = {
    {"",          SYSCALL_EXECUTE,      call_routine_execute,     SYSCALL_FLAG_COMMAND},
    {"ls",        SYSCALL_LIST,         call_routine_ls,          SYSCALL_FLAG_COMMAND},
    {"cd",        SYSCALL_CHDIR,        call_routine_cd,          SYSCALL_FLAG_COMMAND},
    
    //{"cp",        SYSCALL_COPY,         call_routine_copy,        SYSCALL_FLAG_COMMAND},
    
    //{"mk",        SYSCALL_MAKE,         call_routine_mk,          SYSCALL_FLAG_COMMAND},
    //{"mkdir",     SYSCALL_MAKEDIR,      call_routine_mkdir,       SYSCALL_FLAG_COMMAND},
    //{"rm",        SYSCALL_REMOVE,       call_routine_rm,          SYSCALL_FLAG_COMMAND},
    
    //{"boot",      SYSCALL_BOOT,         call_routine_boot,        SYSCALL_FLAG_COMMAND},
    //{"format",    SYSCALL_FORMAT,       call_routine_format,      SYSCALL_FLAG_COMMAND},
    //{"chkdsk",    SYSCALL_CHKDSK,       call_routine_chkdsk,      SYSCALL_FLAG_COMMAND},
    
    //{"graph",     SYSCALL_GRAPHICS,     call_routine_graph,       SYSCALL_FLAG_COMMAND},
    
    //{"type",      SYSCALL_TYPE,         call_routine_type,        SYSCALL_FLAG_COMMAND},
    
};

static const uint32_t command_table_count = sizeof(command_table) / sizeof(command_table[0]);

CommandFunction syscall_get_function(uint16_t index) {
    if (index >= command_table_count) 
        return NULL;
    return command_table[index].function;
}

const char* syscall_get_function_name(uint16_t index) {
    if (index >= command_table_count) 
        return NULL;
    return command_table[index].name;
}

uint8_t syscall_get_flags(uint16_t index) {
    if (index >= command_table_count) 
        return 0;
    return command_table[index].flags;
}

uint16_t syscall_get_count(void) {
    return command_table_count;
}

int syscall(uint16_t id, char** args) {
    for (uint16_t i=0; i < command_table_count; i++) {
        if (command_table[i].id != id) 
            continue;
        
        int agr_count=0;
        if (args != NULL) {
            for (uint32_t a=0; a < 16; a++) {
                if (args[a][0] == '\0') 
                    break;
                agr_count++;
            }
        }
        
        return command_table[i].function(agr_count, args);
    }
    return 0;
}
