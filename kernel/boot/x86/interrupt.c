#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/io.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/arch/x86/drivers/ps2.h>
#include <kernel/panic/panic_error.h>

#include <kernel/dwm/dwm.h>
#include <kernel/console/keyboard.h>
#include <kernel/scheduler/scheduler.h>
extern void scheduler_context(void);
extern void scheduler_yield_context(void);

extern void isr_dummy(void);
extern void isr_div_zero(void);
extern void isr_mouse(void);
extern void isr_keyboard(void);
extern void isr_timer(void);
extern void isr_page_fault(void);
extern void isr_general_fault(void);

void pic_remap(void);

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
    
    idt_set_gate(0,    (uint32_t)isr_div_zero,              0x08, 0x8E);
    idt_set_gate(0x20, (uint32_t)scheduler_context,     0x08, 0x8E);
    idt_set_gate(0x80, (uint32_t)scheduler_yield_context,   0x08, 0x8E);
    
    idt_set_gate(0x0E, (uint32_t)isr_page_fault,            0x08, 0x8E);
    idt_set_gate(0x0D, (uint32_t)isr_general_fault,         0x08, 0x8E);
    
    idt_set_gate(0x21, (uint32_t)isr_keyboard,              0x08, 0x8E);
    idt_set_gate(0x2C, (uint32_t)isr_mouse,                 0x08, 0x8E);
    
    pic_remap();
    
    // Unmask Master PIC (0x21)
    // Bit 0 = Timer (IRQ 0), Bit 1 = Keyboard (IRQ 1), Bit 2 = Cascade (IRQ 2)
    // 0xF8 = 1111 1000 in binary
    outb(0x21, 0xF8);
    
    // Unmask Slave PIC (0xA1)
    // Bit 4 = Mouse (IRQ 12 is IRQ 4 on the Slave PIC: 12 - 8 = 4)
    // 0xEF = 1110 1111 in binary
    outb(0xA1, 0xEF);
    
    __asm__ __volatile__("lidt (%0)" : : "r" (&idtp));
}

static inline void interrupt_end(void) {outb(0x20, 0x20);}
static inline void slave_interrupt_end(void) {outb(0xA0, 0x20); interrupt_end();}

void isr_callback_div_zero_handler(void) {
    
    interrupt_end();
}

void c_dummy_handler(void) {
    
    interrupt_end();
}

void isr_callback_fault_handler(uint32_t error_code, uint32_t faulting_address, uint8_t type) {
    
    kernel_crashout(error_code, faulting_address, type, "");
    
    while(1);
    interrupt_end();
}

void keyboard_handler_c(void) {
    if (ps2_check_keyboard()) {
        uint16_t last_key_pressed = kb_getc();
        
        dwm_post_message(dwm_window_get_focus(), DWM_EVENT_KEYBOARD, last_key_pressed, 0);
    }
    interrupt_end();
}

void mouse_handler_c(void) {
    mouse_event_handler();
    
    slave_interrupt_end();
}
