#include <kernel/main.h>

int main(void) {
    
    // Zero the system bus
    bus_initiate();
    
    // Allow some time to stabilize for the board level IO and logic
    _delay_ms(1000);
    
    // Initiate core kernel systems (in this order)
    InitBakedDrivers();           // Baked device drivers
    InitiateDeviceTable();        // Hardware device table
    KernelVectorTableInit();      // Hardware interrupt vector table
    
    cliInit();
    
    // Check amount of external memory
    AllocateExternalMemory();
    
    // Speaker beep error codes
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    
    // Check RAM error
    if (kAllocGetTotal() < 1024) {
        
        for (uint8_t a=0; a < 6; a++) 
            sysbeep();
        
        _delay_ms(500);
        sysbeep();
        
        _delay_ms(500);
        sysbeep();
        
        kThrow(HALT_OUT_OF_MEMORY, 0x00000);
    }
    
    // Normal beep, all clear to continue
    sysbeep();
    
#endif
    
    // Initiate kernel sub systems
    
    fsInit();         // File system
    kInit();          // Kernel environment
    
    registerCommandLS();
    registerCommandCD();
    
    //registerCommandEDIT();
    //registerCommandASM();
    
    //registerCommandMK();
    
    //registerCommandType();
    //registerCommandList();
    //registerCommandRM();
    //registerCommandTASK();
    
    //registerCommandTest();
    
    //registerCommandGRAPHICS();
    
    //registerCommandRN();
    //registerCommandCOPY();
    
    //registerCommandNet();
    
    //registerCommandFormat();
    //registerCommandBoot();
    
    #define DONT_INCLUDE_CONSOLE_COMMANDS
    
    
#ifndef DONT_INCLUDE_CONSOLE_COMMANDS
    
  #ifdef INCLUDE_KERNEL_APPLICATIONS
    
    registerCommandCAP();
    registerCommandList();
    registerCommandTASK();
    registerCommandType();
    
    registerCommandLD();
    
    //registerCommandEDIT();
    //registerCommandAssembly();
    
    registerCommandCLS();
    
    registerCommandTest();
    
  #endif
    
  #ifdef INCLUDE_NETWORK_APPLICATIONS
    
    registerCommandNet();
    //registerCommandFTP();
    
  #endif
    
  #ifdef INCLUDE_FILE_SYSTEM_APPLICATIONS
    
    registerCommandLS();
    registerCommandCD();
    
    //registerCommandMK();
    //registerCommandRM();
    registerCommandRN();
    //registerCommandMKDIR();
    registerCommandCOPY();
    
    registerCommandAttribute();
    //registerCommandRepair();
    registerCommandFormat();
    
  #endif
    
#endif
    
    //
    // Boot the kernel
    
#ifdef BOARD_RETRO_AVR_X4_REV1
    
    // Set keyboard interrupt handler
    //SetInterruptVector(4, (void(*)())cliRunShell);
    
#endif
    
    // Drop the initial command prompt
    printPrompt();
    
    // Set the interrupt callback
    SetHardwareInterruptService( _ISR_hardware_service_routine );
    
    // Enable hardware interrupt handling
    //  Trigger on the HIGH to LOW transition of PIN2
    InterruptStartHardware();
    
    // Prepare the scheduler and its 
    // associated hardware interrupts
    SchedulerStart();
    
    InterruptStartScheduler();
    InterruptStartTimeCounter();
    
    EnableGlobalInterrupts();
	
    while(1) {
        
#ifdef BOARD_RETROBOARD_REV2
        
        DisableGlobalInterrupts();
        
        cliRunShell();
        
        EnableGlobalInterrupts();
        
#endif
        
        continue;
    }
    
    InterruptStopTimerCounter();
    InterruptStopScheduler();
    InterruptStopHardware();
    
    SchedulerStop();
    
    return 0;
}

