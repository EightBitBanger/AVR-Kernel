#include <avr/io.h>
#include <stdlib.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/arch/avr/io.h>

void _ISR_shell_service(void) {
    cliRunShell();
}

int main(void) {
    mmio_initiate();
    
    // Allow some time to stabilize the board level IO and logic
    _delay_ms(1000);
    
    // Initiate core kernel (in this order)
    InitBakedDrivers();           // Static kernel level device drivers
    InitiateDeviceTable();        // Detect hardware on the bus
    InitHardwareInterruptTable(); // Initiate hardware interrupts
    
    cliInit();
    
    AllocateExternalMemory();
    
    // Check RAM error
    if (kAllocGetTotal() < 1024) 
        kThrow(HALT_OUT_OF_MEMORY, 0x00000);
    
    // Initiate kernel sub systems
    
    fsInit();         // File system
    kInit();          // Kernel environment
    
    ConsoleClearScreen(' ');
    ConsoleSetCursor(0, 0);
    InitiateRouter();
    
    // Console command functions
    RegisterCommands();
    
    // Set keyboard interrupt handler
    //SetInterruptVector(0, (void(*)())cliRunShell);
    
    // Drop the initial command prompt
    printPrompt();
    
    // Set master interrupt callback
    SetHardwareInterruptService((void(*)())cliRunShell);
    
    // Enable hardware interrupt handling
    //  Trigger on the HIGH to LOW transition of PIN2
    InterruptStartHardware();
    
    // Prepare the scheduler and its 
    // associated hardware interrupts
    SchedulerStart();
    
    InterruptStartScheduler();
    InterruptStartTimeCounter();
    
    
    //EnableGlobalInterrupts();
	
    while(1) {}
    
    InterruptStopScheduler();
    InterruptStopHardware();
    
    SchedulerStop();
    
    return 0;
}

