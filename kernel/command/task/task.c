#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/task/task.h>


void functionTASK(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgTaskFound[]     = "Task stopped";
    uint8_t msgTaskNotFound[]  = "Task not found";
    
    int32_t taskIndex = TaskFind(param, param_length);
    
    if (TaskDestroy(taskIndex) == 0) {
        print(msgTaskNotFound, sizeof(msgTaskNotFound));
        printLn();
        return;
    }
    
    print(msgTaskFound, sizeof(msgTaskFound));
    printLn();
    
    
    /*
    
    uint8_t msgTaskFound[]     = "Task stopped";
    uint8_t msgTaskNotFound[]  = "Task not found";
    
    lowercase(&param[0]);
    
    // List tasks
    if ((param[0] == ' ') & (param[1] == ' ')) {
        
        struct ProcessDescription proc_desc;
        
        
        for (uint8_t i=0; i < TASK_LIST_SIZE; i++) {
            
            if (GetProcInfo(i, &proc_desc) == 0) 
                continue;
            
            if (proc_desc.block == 0) 
                continue;
            
            uint8_t currentMode = VirtualAccessGetMode();
            
            if (proc_desc.privilege == TASK_PRIVILEGE_KERNEL)  VirtualAccessSetMode( VIRTUAL_ACCESS_MODE_KERNEL );
            if (proc_desc.privilege == TASK_PRIVILEGE_SERVICE) VirtualAccessSetMode( VIRTUAL_ACCESS_MODE_SERVICE );
            if (proc_desc.privilege == TASK_PRIVILEGE_USER)    VirtualAccessSetMode( VIRTUAL_ACCESS_MODE_USER );
            
            // Type
            printChar( proc_desc.privilege );
            printSpace(1);
            
            // Name
            uint8_t filename[FILE_NAME_LENGTH];
            
            VirtualBegin();
            
            fsFileGetName(proc_desc.block, filename);
            
            VirtualEnd();
            
            print(filename, TASK_NAME_LENGTH_MAX);
            printSpace(2);
            
            // Priority
            if (proc_desc.priority == TASK_PRIORITY_NORMAL) {
                
                printc("normal", 7);
                
            } else {
                
                if (proc_desc.priority == TASK_PRIORITY_HALT) {
                    
                    printc("halted", 7);
                    
                } else {
                    
                    if (proc_desc.priority < TASK_PRIORITY_NORMAL) printc("high", 5);
                    if (proc_desc.priority > TASK_PRIORITY_NORMAL) printc("low", 4);
                    
                }
            }
            
            VirtualAccessSetMode(currentMode);
            
            printLn();
            
        }
        
        return;
    }
    
    // Kill a task
    if ((param[0] == 'k') & (param[1] == ' ')) {
        
        if (TaskFind(&param[2], param_length - 2) != 0) {
            
            print(msgTaskFound, sizeof(msgTaskFound));
            printLn();
            
            TaskKill(&param[2], param_length - 2);
            
        } else {
            
            print(msgTaskNotFound, sizeof(msgTaskNotFound));
            printLn();
        }
        
        return;
    }
    */
    return;
}

void registerCommandTASK(void) {
    
    uint8_t taskCommandName[] = "stop";
    
    ConsoleRegisterCommand(taskCommandName, sizeof(taskCommandName), functionTASK);
    
    return;
}
