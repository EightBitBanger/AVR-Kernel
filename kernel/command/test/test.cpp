extern "C" {
#include <kernel/kernel.h>
#include <kernel/command/test/test.h>
}


void testTask(uint8_t event) {
    
}


void functionTest(uint8_t* param, uint8_t param_length) {
    
	uint8_t taskName[] = "newtask\0";
	if (TaskCreate(taskName, testTask, TASK_PRIORITY_NORMAL, TASK_PRIVILEGE_USER, TASK_TYPE_TSR) >= 0) {
        uint8_t passedTaskName[] = "passed";
        print(passedTaskName, sizeof(passedTaskName));
        printLn();
	} else {
	    uint8_t errorTaskName[] = "error";
        print(errorTaskName, sizeof(errorTaskName));
        printLn();
	}
	
    return;
}

void registerCommandTest(void) {
    uint8_t testCommandName[] = "test";
    ConsoleRegisterCommand(testCommandName, sizeof(testCommandName), functionTest);
}
