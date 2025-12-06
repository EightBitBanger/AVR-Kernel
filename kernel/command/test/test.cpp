extern "C" {
#include <kernel/kernel.h>
#include <kernel/command/test/test.h>
}

void functionTest(uint8_t* param, uint8_t param_length) {
    
    VirtualBegin();
    
    VirtualEnd();
    
    return;
}

void registerCommandTest(void) {
    uint8_t testCommandName[] = "test";
    ConsoleRegisterCommand(testCommandName, sizeof(testCommandName), functionTest);
}
