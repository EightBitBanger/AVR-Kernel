#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/knode.h>

#include <string.h>

#include <kernel/syscall/graphix.h>


ISR(INT2_vect) {
    
    kb_isr_callback();
}

//ISR(TIMER0_COMPA_vect) { timer_isr_callback(); }      // Timer
ISR(TIMER1_COMPA_vect) { scheduler_isr_callback(); }  // Scheduler

uint32_t detect_external_memory(void);

int main() {
    mmio_address_zero();
    mmio_control_zero();
    
    _delay_ms(1000);
    
    interrupt_init();
    
    char keyboard_string[16];
    char prompt_string[16];
    
    kb_init();
    
    console_init(keyboard_string, prompt_string);
    
    /*
    struct Bus bus;
    bus.read_waitstate  = 2;
    bus.write_waitstate = 1;
    
    {
    char block[] = "What the fuck?";
    
    for (uint8_t i=0; i < sizeof(block); i++) 
        mmio_writeb(&bus, 0x00000000 + i, &block[i]);
    
    
    }
    
    
    {
    char block[32];
    memset(block, 0x00, sizeof(block));
    
    mmio_read_block(&bus, 0x00000000, block, 32);
    
    block[16] = '\0';
    
    print(block);
    }
    
    
    while(1);
    */
    
    // Allocate total memory
    
    uint32_t block_size = 32UL;
    uint32_t total_memory = detect_external_memory();
    const uint32_t ALLOCATOR_MAX = total_memory;
    
    uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    kmalloc_bitmap_set(bitmap, 0);
    heap_set_base_address(0x00000000);
    
    heap_init(block_size, ALLOCATOR_MAX);
    
    print("kernel v0.0.0\n");
    kernel_init();
    
    x4_init();
    
    scheduler_init();
    
    print_prompt();
    interrupt_enable();
    
    while (1) {
        
        interrupt_disable();
            scheduler_handler();
        interrupt_enable();
        
        kb_event_handler();
        
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
            console_set_cursor_position(0, 0);
            print_int(total_memory);
        }
    }
    console_set_cursor_position(0, 0);
    print_int(total_memory);
    print(" bytes free\n");
    
    return total_memory;
}
