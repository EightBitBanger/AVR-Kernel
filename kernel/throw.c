#include <avr/io.h>

#include <kernel/throw.h>
#include <kernel/pointers.h>
#include <kernel/syscalls/print/print.h>

uint8_t msgErrorEvent[]   = "Fatal kernel error";
uint8_t msgHardwareAddr[] = "0x";

uint8_t msgSegFault[] = "SIG-SEGV";
uint8_t msgBadAlloc[] = "BAD-ALLOC";
uint8_t msgOutOfMem[] = "OUT-OF-MEM";
uint8_t msgProgHang[] = "APP-HANG";

void kHalt(void) {while(1) {__asm__("nop");}}


void kThrow(long int errorCode, uint32_t hardwareAddress) {
    
    ConsoleClearScreen(' ');
    
    ConsoleSetCursor(0, 0);
    print(msgErrorEvent, sizeof(msgErrorEvent));
    
    ConsoleSetCursor(6, 0);
    
    switch (errorCode) {
        
        case HALT_BAD_ALLOCATION: {print(msgBadAlloc, sizeof(msgBadAlloc)); break;}
        case HALT_SEGMENTATION_FAULT: {print(msgSegFault, sizeof(msgSegFault)); break;}
        case HALT_OUT_OF_MEMORY: {print(msgOutOfMem, sizeof(msgOutOfMem)); break;}
        case HALT_APP_HANG: {print(msgProgHang, sizeof(msgProgHang)); break;}
        
    }
    
    ConsoleSetCursor(7, 0);
    print(msgHardwareAddr, sizeof(msgHardwareAddr));
    
    // Print hardware address as hex
    union Pointer addressPtr;
    addressPtr.address = errorCode + hardwareAddress;
    
    uint8_t hexstr[2];
    
    for (uint8_t i=0; i < 4; i++) {
        
        ConsoleSetCursor(7, 2 + (2 * i));
        
        int_to_hex_string(addressPtr.byte_t[3 - i], hexstr);
        print(hexstr, 3);
    }
    
    ConsoleCursorDisable();
    kHalt();
    
    return;
}
