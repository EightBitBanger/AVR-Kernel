#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/device/driver.h>
#include <kernel/device/device.h>

//#include <kernel/fs/fs.h>

#include <kernel/syscall/print.h>

#include <kernel/kernel.h>
#include <kernel/delay.h>

#include <string.h>

ISR(INT2_vect) {
    
    kb_isr_callback();
}

uint32_t allocate_external_memory(void);

int main() {
    mmio_address_zero();
    mmio_control_zero();
    
    _delay_ms(1000);
    
    interrupt_init();
    console_init();
    
    uint32_t total_memory = 0UL;
    uint32_t block_size   = 32UL;
    
    total_memory = allocate_external_memory();
    const uint32_t ALLOCATOR_MAX = total_memory / 2;
    
    uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    kmalloc_bitmap_set(bitmap);
    heap_set_base_address(0x00000000);
    
    heap_init(block_size, ALLOCATOR_MAX);
    
    print("kernel v0.0.0\n");
    
    kernel_init();
    
    kmalloc_bitmap_write();
    
    
    print_prompt();
    interrupt_enable();
    
    while (1) {
        kb_handler();
    }
    
}


uint32_t allocate_external_memory(void) {
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
            console_set_position(0, 0);
            print_int(total_memory);
        }
    }
    console_set_position(0, 0);
    print_int(total_memory);
    print(" bytes free\n");
    
    return total_memory;
}
