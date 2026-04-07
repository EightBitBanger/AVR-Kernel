#include <kernel/syscall/print.h>
#include <kernel/syscall/console.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

void process_command(char* keyboard_str) {
    const char* functionName = "ls";
    
    char* command = strtok(keyboard_str, " ");
    
    if (strcmp(command, functionName) == 0) {
        print(command);
        print("\n");
    }
    
    //while (token != NULL) token = strtok(NULL, " ");
    
}

