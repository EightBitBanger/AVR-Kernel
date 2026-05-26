#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/util/timer.h>

//
// Using PIT timer for rough sleep timing

void delay_ms(uint32_t ms) {
    // 1. Calculate how many PIT ticks equal 1 millisecond.
    // 1193182 Hz / 1000 ms = ~1193 ticks per millisecond
    const uint32_t ticks_per_ms = 1193;
    
    // Save the original PC speaker/gate state so we don't permanently alter hardware settings
    uint8_t original_gate = inb(0x61);
    
    while (ms > 0) {
        // Determine the chunk size for this iteration (max 50ms per hardware pass)
        uint32_t chunk_ms = (ms > 50) ? 50 : ms;
        uint16_t count = (uint16_t)(chunk_ms * ticks_per_ms);
        
        // Configure PIT Channel 2: Mode 0 (Interrupt on terminal count), Write LSB then MSB
        outb(0x43, 0xB0); 
        outb(0x42, (uint8_t)(count & 0xFF));        // Low byte
        outb(0x42, (uint8_t)((count >> 8) & 0xFF)); // High byte
        
        // Enable PIT Channel 2 counting by pulling bits 0 and 1 high
        uint8_t gate = inb(0x61);
        outb(0x61, gate | 0x03);
        
        // Poll bit 5 of port 0x61 until it goes high, indicating the countdown finished
        while (!(inb(0x61) & 0x20));
        
        ms -= chunk_ms;
    }
    
    // Restore the original gate state to turn off the timer clean
    outb(0x61, original_gate & ~0x03);
}
