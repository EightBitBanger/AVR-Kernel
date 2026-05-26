#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/io.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/mouse.h>

extern void isr_dummy(void);
extern void isr_div_zero(void);
extern void isr_mouse(void);
extern void isr_keyboard(void);

void pic_remap(void);

// Create an array of 256 entries and the pointer
struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void idt_initiate(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;
    
    for (int i = 0; i < 256; i++) 
        idt_set_gate(i, (uint32_t)isr_dummy, 0x08, 0x8E);
    
    idt_set_gate(0,    (uint32_t)isr_div_zero,  0x08, 0x8E);
    //idt_set_gate(0x21, (uint32_t)isr_keyboard,    0x08, 0x8E);
    //idt_set_gate(0x2C, (uint32_t)isr_mouse,       0x08, 0x8E);
    
    __asm__ __volatile__("lidt (%0)" : : "r" (&idtp));
    
    // Remap vector 8 to prevent crash on hardware timer
    pic_remap();
    
    //outb(0x21, inb(0x21) & ~(1 << 1));
    
    __asm__ __volatile__("sti");
}

void c_interrupt_handler(void) {
    
}
