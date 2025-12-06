#include <kernel/main.h>

void _ISR_shell_service(void) {
    //_delay_ms(90);
    
    //MutexLock(&isr_mux);
    
    //uint8_t msgTest[] = "A";
    //print(msgTest, sizeof(msgTest));
    //printSpace(1);
    //printLn();
    
    //MutexUnlock(&isr_mux);
    
    cliRunShell();
    
}

int main(void) {
    
    // Zero the system bus
    bus_initiate();
    
    // Allow some time to stabilize for the board level IO and logic
    _delay_ms(1000);
    
    // Initiate core kernel systems (in this order)
    InitBakedDrivers();           // Static kernel level device drivers
    InitiateDeviceTable();        // Hardware device table
    KernelVectorTableInit();      // Hardware interrupt vector table
    
    cliInit();
    
    // Check amount of external memory
    AllocateExternalMemory();
    
    // Speaker beep error codes
    
    // Check RAM error
    if (kAllocGetTotal() < 1024) 
        kThrow(HALT_OUT_OF_MEMORY, 0x00000);
    
    // Initiate kernel sub systems
    
    fsInit();         // File system
    kInit();          // Kernel environment
    
    // Console command functions
    
    registerCommandLS();
    registerCommandCD();
    
    //registerCommandTest();
    
    //registerCommandEDIT();
    //registerCommandASM();
    
    //registerCommandMK();
    
    //registerCommandType();
    //registerCommandList();
    //registerCommandRM();
    //registerCommandTASK();
    
    //registerCommandGRAPHICS();
    
    //registerCommandRN();
    //registerCommandCOPY();
    
    //registerCommandNet();
    
    //registerCommandFormat();
    //registerCommandBoot();
    
    
    // Boot the kernel
    
    // Set keyboard interrupt handler
    //SetInterruptVector(0, (void(*)())cliRunShell);
    
    // Drop the initial command prompt
    printPrompt();
    
    // Set master interrupt callback
    SetHardwareInterruptService(2, &cliRunShell);
    
    // Enable hardware interrupt handling
    //  Trigger on the HIGH to LOW transition of PIN2
    InterruptStartHardware();
    
    // Prepare the scheduler and its 
    // associated hardware interrupts
    //SchedulerStart();
    
    //InterruptStartScheduler();
    //InterruptStartTimeCounter();
    
    EnableGlobalInterrupts();
	
    while(1) {}
    
    //InterruptStopTimerCounter();
    //InterruptStopScheduler();
    //InterruptStopHardware();
    
    //SchedulerStop();
    
    return 0;
}

