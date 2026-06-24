#ifndef _KERNEL_INTERRUPT_H_
#define _KERNEL_INTERRUPT_H_

#include <stdint.h>
#include <stdbool.h>

struct idt_entry {
    uint16_t base_low;  // The lower 16 bits of the ISR address
    uint16_t sel;       // Kernel segment selector
    uint8_t  always0;   // This must always be 0
    uint8_t  flags;     // More flags (Type, DPL, Present)
    uint16_t base_high; // The upper 16 bits of the ISR address
} __attribute__((packed));

// IDT pointer for "lidt"
struct idt_ptr {
    uint16_t limit;     // Size of the IDT array minus 1
    uint32_t base;      // The linear address of the first element
} __attribute__((packed));


void idt_init(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif
