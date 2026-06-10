#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/io.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/panic/panic_error.h>

extern void isr_dummy(void);
extern void isr_div_zero(void);
extern void isr_mouse(void);
extern void isr_keyboard(void);
extern void isr_timer(void);
extern void isr_page_fault(void);
extern void isr_general_fault(void);

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

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;
    
    for (int i = 0; i < 256; i++) 
        idt_set_gate(i, (uint32_t)isr_dummy, 0x08, 0x8E);
    
    idt_set_gate(0,    (uint32_t)isr_div_zero,        0x08, 0x8E);
    idt_set_gate(0x20, (uint32_t)isr_timer,           0x08, 0x8E);
    idt_set_gate(0x0E, (uint32_t)isr_page_fault,      0x08, 0x8E);
    idt_set_gate(0x0D, (uint32_t)isr_general_fault,   0x08, 0x8E);
    
    //idt_set_gate(0x21, (uint32_t)isr_keyboard,    0x08, 0x8E);
    //idt_set_gate(0x2C, (uint32_t)isr_mouse,       0x08, 0x8E);
    
    __asm__ __volatile__("lidt (%0)" : : "r" (&idtp));
    
    pic_remap();
    
    // Unmask IRQ 0 (Timer) and IRQ 1 (Keyboard) on the PIC
    // Bit 0 = Timer, Bit 1 = Keyboard. 0 means UNMASKED (enabled)
    outb(0x21, 0xFC);
    
    __asm__ __volatile__("sti");
}


void isr_callback_div_zero_handler(void) {
    
    outb(0x20, 0x20); // End of Interrupt
}

void c_dummy_handler(void) {
    
    outb(0x20, 0x20); // End of Interrupt
}

void isr_callback_fault_handler(uint32_t error_code, uint32_t faulting_address, uint8_t type) {
    
    kernel_crashout(error_code, faulting_address, type, "");
    
    while(1);
}
