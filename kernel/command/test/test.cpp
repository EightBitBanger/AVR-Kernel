extern "C" {
#include <kernel/kernel.h>
#include <kernel/command/test/test.h>
}

void functionTest(uint8_t* param, uint8_t param_length) {
    
    uint8_t driverName[] = "network";
    Driver* networkDriver = GetDriverByName(driverName, sizeof(driverName)-1);
    
    if (networkDriver == nullptr) {
        uint8_t driverError[] = "Driver not found";
        print(driverError, sizeof(driverError));
        printLn();
        return;
    }
    
    uint8_t buffer[30];
    for (unsigned int i=0; i < 30; i++) 
        bus_read_byte(&networkDriver->device.bus, networkDriver->device.hardware_address + i, &buffer[i]);
    
    print(buffer, 30);
    printLn();
    
    return;
}


void registerCommandTest(void) {
    
    uint8_t testCommandName[] = "test";
    
    ConsoleRegisterCommand(testCommandName, sizeof(testCommandName), functionTest);
    
    return;
}
