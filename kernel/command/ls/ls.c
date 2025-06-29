#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/command/ls/ls.h>

void functionLS(uint8_t* param, uint8_t param_length) {
    uint32_t deviceAddress = fsDeviceGetCurrent();
    
    struct Partition part = fsDeviceOpen( deviceAddress );
    DirectoryHandle currentDirectory = fsWorkingDirectoryGetCurrent();
    
    vfsList(part, currentDirectory);
    return;
}

void registerCommandLS(void) {
    uint8_t lsCommandName[] = "ls";
    ConsoleRegisterCommand(lsCommandName, sizeof(lsCommandName), functionLS);
    return;
}
