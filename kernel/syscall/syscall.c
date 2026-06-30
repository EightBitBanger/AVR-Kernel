#include <kernel/syscall.h>
#include <kernel/syscall/commands.h>

#include <kernel/console/display.h>

int command_clear_display(int arg_count, char** args);
int command_execute(int arg_count, char** args);

struct CommandFunction {const char* name; uint16_t id; CommandFunction function; uint8_t flags;};
static const struct CommandFunction command_table[] = {
    {"",          SYSCALL_EXECUTE,      command_execute,           KSCF_COMMAND},
    {"cls",       SYSCALL_CLS,          command_clear_display,     KSCF_COMMAND},
    {"ls",        SYSCALL_LIST,         call_routine_lsdir,        KSCF_COMMAND},
    {"cd",        SYSCALL_CHDIR,        call_routine_chdir,        KSCF_COMMAND},
    
    {"mk",        SYSCALL_MAKE,         call_routine_mk,           KSCF_COMMAND},
    {"mkdir",     SYSCALL_MAKEDIR,      call_routine_mkdir,        KSCF_COMMAND},
    {"rm",        SYSCALL_REMOVE,       call_routine_rm,           KSCF_COMMAND},
    {"rn",        SYSCALL_RENAME,       call_routine_rename,       KSCF_COMMAND},
    {"cp",        SYSCALL_COPY,         call_routine_copy,         KSCF_COMMAND},
    
    {"type",      SYSCALL_TYPE,         call_routine_type,         KSCF_COMMAND},
    
    {"format",    SYSCALL_FORMAT,       call_routine_format,       KSCF_COMMAND},
    {"chkdsk",    SYSCALL_CHKDSK,       call_routine_chkdsk,       KSCF_COMMAND},
    
    //{"boot",      SYSCALL_BOOT,         call_routine_boot,         KSCF_COMMAND},
    //{"graph",     SYSCALL_GRAPHICS,     call_routine_graph,        KSCF_COMMAND},
    
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

int command_clear_display(int arg_count, char** args) {
    display_clear();
    display_cursor_set_line(0);
    display_cursor_set_position(0);
}

int command_execute(int arg_count, char** args) {
    print("Execute "); // TODO execute a program by name
    print(args[0]);
    print("\n");
}
