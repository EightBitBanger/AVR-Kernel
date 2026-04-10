#include <kernel/kernel.h>
#include <string.h>
#include <stdint.h>

void kernel_init(void) {
    uint32_t root = create_knode("", 0);
    console_set_directory(root);
    
    uint32_t dev = create_knode("dev", root);
    uint32_t mnt = create_knode("mnt", root);
    
    knode_add_reference(root, dev);
    knode_add_reference(root, mnt);
    
    hardware_identify_devices(dev, mnt);
}

uint32_t create_device(const char* name) {
    struct Device device;
    memset(&device, 0x00, sizeof(struct Device));
    strcpy(device.name, name);
    
    uint32_t device_address = kmalloc(sizeof(struct Device));
    
    kmalloc_set_flags(device_address, KMALLOC_FLAG_DEVICE);
    kmalloc_set_permissions(device_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmem_write(device_address, &device, sizeof(struct Device));
    return device_address;
}

void destroy_device(uint32_t address) {
    kfree(address);
}

uint32_t create_driver(const char* name) {
    struct Driver driver;
    memset(&driver, 0x00, sizeof(struct Driver));
    strcpy(driver.name, name);
    
    uint32_t driver_address = kmalloc(sizeof(struct Driver));
    
    kmalloc_set_flags(driver_address, KMALLOC_FLAG_DRIVER);
    kmalloc_set_permissions(driver_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmem_write(driver_address, &driver, sizeof(struct Driver));
    return driver_address;
}

void destroy_driver(uint32_t address) {
    kfree(address);
}


void hardware_identify_devices(uint32_t knode_device, uint32_t knode_mount) {
    struct Bus bus;
    bus.read_waitstate  = 20;
    bus.write_waitstate = 20;
    
    uint16_t mount_count = 0;
    
    for (uint32_t base = PERIPHERAL_ADDRESS_BEGIN; base <= PERIPHERAL_ADDRESS_END; base += PERIPHERAL_STRIDE) {
        mount_count++;
        uint8_t id=0;
        char name[11]; 
        
        memset(name, 0, sizeof(name));
        
        mmio_readb(&bus, base, &id);
        
        for (unsigned int i = 0; i < 10; i++) {
            uint8_t byte;
            mmio_readb(&bus, base + 1 + i, &byte);
            
            if (byte <= 0x20 || byte > 0x7E) 
                break;
            
            name[i] = (char)byte;
        }
        
        if (name[0] == '\0') 
            continue;
        
        // Mount file system devices
        if (id == 0x13) {
            char device_name[] = " ";
            device_name[0] = (mount_count-1) + 'a';
            
            uint32_t mount_ptr = create_knode(device_name, knode_mount);
            kmalloc_set_flags(mount_ptr, KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT);
            
            // Set the mount point address to the device base address
            knode_add_reference(mount_ptr, base);
            
            knode_add_reference(knode_mount, mount_ptr);
            continue;
        }
        
        struct Device device;
        strcpy(device.name, name);
        
        device.bus.read_waitstate = 20;
        device.bus.write_waitstate = 20;
        
        device.device_id = id;
        device.device_class = 0;
        
        device.hardware_address = base;
        device.hardware_slot = (base - PERIPHERAL_ADDRESS_BEGIN) / PERIPHERAL_STRIDE + 1;
        
        uint32_t device_ptr = create_device(name);
        kmem_write(device_ptr, &device, sizeof(struct Device));
        
        knode_add_reference(knode_device, device_ptr);
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
