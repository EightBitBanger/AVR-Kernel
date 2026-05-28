#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/util/timer.h>

volatile uint64_t current_ms = 0;

void timer_init(void) {
    // 1193182 Hz / 1000 Hz = 1193 divisor
    uint16_t divisor = 1193;
    
    // Command byte: Channel 0, access LoByte/HiByte, Mode 3, Binary
    outb(0x43, 0x36);
    
    // Set divisor
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_get_ms(void) {
    return current_ms;
}

void isr_callback_timer_handler(void) {
    
    current_ms++;
    
    outb(0x20, 0x20); // End of Interrupt
}
