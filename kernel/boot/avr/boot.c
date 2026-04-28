#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/knode.h>

#include <string.h>

uint8_t code_block[] = {
    0x89, 0x01, 0x00, 0xCD, 0x16, 0x83, 0x06, 0x0F, 0x00, 0x00, 0x00, 0xCD, 0x10, 0xCD, 0x20, 0x77, 0x74, 0x66, 0x0D, 0x00, 
};


ISR(INT2_vect) {
    
    kb_isr_callback();
}

//ISR(TIMER0_COMPA_vect) { timer_isr_callback(); }      // Timer
ISR(TIMER1_COMPA_vect) { scheduler_isr_callback(); }  // Scheduler

uint32_t detect_external_memory(void);

void _system_reset_(void) {
    display_clear();
    void(*resetFunc) (void) = 0;
    resetFunc();
}

int main() {
    mmio_address_zero();
    mmio_control_zero();
    
    _delay_ms(300);
    
    interrupt_init();
    
    char keyboard_string[16];
    char prompt_string[16];
    char virtual_key_map[84];
    
    kb_init();
    kb_map_init(virtual_key_map, sizeof(virtual_key_map));
    
    display_init();
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    // Allocate total memory
    
    uint32_t block_size = 32UL;
    uint32_t total_memory = detect_external_memory();
    const uint32_t allocator_max = total_memory;
    
    uint8_t bitmap[allocator_max / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    kmalloc_bitmap_set(bitmap, 0);
    heap_set_base_address(0x00000000);
    
    print("kernel v0.0.0\n");
    
    heap_init(block_size, allocator_max);
    
    kernel_init();
    x4_init();
    scheduler_init();
    
    console_prompt_print();
    interrupt_enable();
    
    while (1) {
        scheduler_handler();
        
        interrupt_enable();
        kb_event_handler();
        
        // Soft reset
        if (kb_vkey_check(VK_CONTROL) && kb_vkey_check(VK_ALT) && kb_vkey_check(VK_DELETE)) {
            _system_reset_();
        }
        
        // Setup
        if (kb_vkey_check(VK_CONTROL) && kb_vkey_check(VK_ALT) && kb_vkey_check(VK_INSERT)) {
            display_clear();
            
            while(1) {
                
                scheduler_handler();
                
                if (kb_vkey_check(VK_ESCAPE)) {
                    _system_reset_();
                }
                
                // Exit
                if (kb_vkey_check(VK_CONTROL) && kb_vkey_check(VK_ALT) && kb_vkey_check(VK_DELETE)) {
                    _system_reset_();
                }
                
            }
        }
        interrupt_disable();
    }
    
}


uint32_t detect_external_memory(void) {
    struct Bus membus;
    membus.read_waitstate  = 2;
    membus.write_waitstate = 1;
    uint8_t pattern = 0x55;
    
    uint16_t counter=0;
    uint32_t total_memory=0;
    for (uint32_t address=MEMORY_ADDRESS_BEGIN; address < MEMORY_ADDRESS_END; address++) {
        uint8_t byte=0;
        mmio_writeb(&membus, address, &pattern);
        mmio_readb(&membus, address, &byte);
        if (byte != pattern) 
            break;
        
        total_memory++;
        counter++;
        if (counter > 1024) {
            counter = 0;
            display_cursor_set_position(0);
            display_cursor_set_line(0);
            print_int(total_memory);
        }
    }
    display_cursor_set_position(0);
    display_cursor_set_line(0);
    
    print_int(total_memory);
    print(" bytes free\n");
    
    return total_memory;
}
