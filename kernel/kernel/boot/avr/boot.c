#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/driver/driver.h>
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
    
    
    {
    
    uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    heap_set_base_address(0x00000000);
    
    kmalloc_bitmap_set(bitmap);
    heap_init(block_size, ALLOCATOR_MAX);
    
    hardware_identify_devices();
    kmalloc_bitmap_write();
    
    
    print("kernel v0.0.0\n");
    
    
    
    
    kmalloc_bitmap_set(bitmap);
    kmalloc_bitmap_read();
    
    struct Device kblock;
    const char* name = "dev";
    strcpy(kblock.device_name, name);
    
    uint32_t block_addr = create_device(&kblock);
    
    
    kmalloc_set_flags(block_addr, KMALLOC_FLAG_FS | KMALLOC_FLAG_READ | KMALLOC_FLAG_WRITE);
    kmalloc_set_type(block_addr, KMALLOC_TYPE_DIRECTORY);
    
    
    kmalloc_bitmap_write();
    
    }
    
    {
    uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    kmalloc_bitmap_set(bitmap);
    kmalloc_bitmap_read();
    
    uint32_t next_addr = KMALLOC_NULL;
    for (;;) {
        next_addr = kmalloc_next(next_addr);
        
        if (next_addr == KMALLOC_NULL) 
            break;
        uint8_t flags = kmalloc_get_flags(next_addr);
        uint8_t type  = kmalloc_get_type(next_addr);
        char* attr = "    ";
        
        if (type & KMALLOC_TYPE_EXECUTABLE)  attr[0] = 'x';
        if (flags & KMALLOC_FLAG_READ)       attr[1] = 'r';
        if (flags & KMALLOC_FLAG_WRITE)      attr[2] = 'w';
        
        //if (flags & KMALLOC_FLAG_FS) {
            struct Device device;
            kmem_read(next_addr, &device, sizeof(struct Device));
            
            print_int(next_addr);
            print(" ");
            
            //print(attr);
            print( device.device_name );
            
            size_t namelen = display_get_width() - (strlen(device.device_name) + 9);
            
            if (type & KMALLOC_TYPE_DIRECTORY) {
                for (size_t i=0; i < namelen; i++) 
                    print(" ");
                print("<DIR>");
            }
            
            print("\n");
        //}
    }
    }
    
    
    
    
    
    /*
    uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
    memset(bitmap, 0x00, sizeof(bitmap));
    
    
    uint32_t device_address = fs_device_open(0x00000, bitmap, 32UL, ALLOCATOR_MAX);
    
    struct Device device;
    char* name = "fuck";
    strcpy(device.device_name, name);
    
    fs_create_file(&device, sizeof(struct Device));
    
    fs_bitmap_write();
    */
    
    
    
    
    
    print_prompt();
    
    interrupt_enable();
    
    while (1) {
        /*
        uint8_t bitmap[ALLOCATOR_MAX / block_size / 8UL];
        memset(bitmap, 0x00, sizeof(bitmap));
        
        kmalloc_bitmap_set(bitmap);
        kmalloc_bitmap_read();
        
        
        struct Device kblock;
        const char* name = "dev";
        
        strcpy(kblock.device_name, name);
        
        uint32_t block_addr = create_device(&kblock);
        
        kmalloc_set_flags(block_addr, KMALLOC_FLAG_FS | KMALLOC_FLAG_READ | KMALLOC_FLAG_WRITE);
        kmalloc_set_type(block_addr, KMALLOC_TYPE_DIRECTORY);
        
        destroy_device(block_addr);
        */
        
        //kmalloc_bitmap_write();
        
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
            console_set_cursor(0, 0);
            print_int(total_memory);
        }
    }
    console_set_cursor(0, 0);
    print_int(total_memory);
    print(" bytes free\n");
    
    return total_memory;
}
