#include <kernel/kernel.h>
#include <string.h>

uint32_t create_device(struct Device* device) {
    uint32_t device_address = kmalloc(sizeof(struct Device));
    
    kmalloc_set_flags(device_address, KMALLOC_FLAG_DEVICE | KMALLOC_FLAG_READ | KMALLOC_FLAG_WRITE);
    
    kmalloc_set_type(device_address, 0);
    
    kmem_write(device_address, device, sizeof(struct Device));
    return device_address;
}

void destroy_device(uint32_t address) {
    kfree(address);
}


void hardware_identify_devices(void) {
    struct Bus bus;
    bus.read_waitstate  = 20;
    bus.write_waitstate = 20;
    
    for (uint32_t base = PERIPHERAL_ADDRESS_BEGIN; base <= PERIPHERAL_ADDRESS_END; base += PERIPHERAL_STRIDE) {
        uint8_t id=0;
        char name[11]; 
        memset(name, 0, sizeof(name));
        
        for (unsigned int i = 0; i < 10; i++) {
            uint8_t byte;
            mmio_readb(&bus, base + 1 + i, &byte);
            
            if (byte <= 0x20 || byte > 0x7E) 
                break;
            
            name[i] = (char)byte;
        }
        
        if (name[0] != '\0') {
            struct Device device;
            strcpy(device.device_name, name);
            
            device.bus.read_waitstate = 20;
            device.bus.write_waitstate = 20;
            
            mmio_readb(&bus, base, &id);
            
            device.device_id = id - 6;
            device.device_class = 0;
            
            device.hardware_address = base;
            device.hardware_slot = (base - PERIPHERAL_ADDRESS_BEGIN) / PERIPHERAL_STRIDE + 1;
            
            create_device(&device);
        }
    }
}

uint32_t device_get_hardware_address(const char* name) {
    struct Bus bus;
    bus.read_waitstate  = 20;
    bus.write_waitstate = 20;
    
    for (uint32_t base = PERIPHERAL_ADDRESS_BEGIN; base <= PERIPHERAL_ADDRESS_END; base += PERIPHERAL_STRIDE) {
        uint8_t id=0;
        char device_name[11]; 
        memset(device_name, 0, sizeof(device_name));
        
        mmio_readb(&bus, base, &id);
        
        for (unsigned int i = 0; i < 10; i++) {
            uint8_t byte;
            mmio_readb(&bus, base + 1 + i, &byte);
            
            if (byte <= 0x20 || byte > 0x7E) 
                break;
            
            device_name[i] = (char)byte;
        }
        
        if (device_name[0] != '\0') {
            if (strcmp(device_name, name) == 0) 
                return base;
        }
    }
    return 0;
}
