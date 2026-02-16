#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/syscall/beep.h>

void beep(uint16_t duration, uint16_t frequency) {
    struct Bus spkBus;
    spkBus.read_waitstate  = 1000;
    spkBus.write_waitstate = 1000;
    
    for (uint16_t i=0; i < duration; i++) {
        mmio_write_byte(&spkBus, 0x60000, 0xff);
        for (uint16_t i=0; i < frequency / 16; i++) 
            __asm__("nop");
        continue;
    }
}

void sysbeep(void) {
    beep(300, 14000);
}

void sysbeepFatalError(void) {
    
    for (uint8_t a=0; a < 6; a++) 
        sysbeep();
    _delay_ms(500);
    sysbeep();
    _delay_ms(500);
    sysbeep();
}
