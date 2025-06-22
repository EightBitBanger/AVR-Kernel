#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/mem/mem.h>

void functionMEM(uint8_t* param, uint8_t param_length) {
    
    uint32_t allocsTotal = kAllocationsGetSize();
    
    printInt( allocsTotal );
    printSpace( 1 );
    
    uint8_t msgBytesTotal[] = "bytes";
    print(msgBytesTotal, sizeof(msgBytesTotal));
    
    printLn();
    
    return;
}

void registerCommandMEM(void) {
    
    uint8_t memCommandName[] = "mem";
    
    ConsoleRegisterCommand(memCommandName, sizeof(memCommandName), functionMEM);
    
    return;
}
