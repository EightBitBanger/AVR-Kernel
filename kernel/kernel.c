#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/syscall.h>

#include <string.h>
#include <stdint.h>


struct KernelSystemObject {
    struct WorkingDirectory fs_current;
    
    
};
uint32_t k_system_object;


void kernel_get_system_object(void* object, uint32_t kso_sub_type, uint32_t kso_size) {
    kmalloc_read(k_system_object + kso_sub_type, (uint8_t*)object, kso_size);
}

void kernel_set_system_object(void* object, uint32_t kso_sub_type, uint32_t kso_size) {
    kmalloc_write(k_system_object + kso_sub_type, (uint8_t*)object, kso_size);
}


void kernel_init(void) {
    k_system_object = kmalloc(1024 * 4);
    
    uint32_t root_node = create_knode("", 0);
    
    struct WorkingDirectory fs_current;
    fs_current.current_directory = root_node;
    fs_current.mount_device      = FS_NULL;
    fs_current.mount_directory   = FS_NULL;
    fs_current.mount_root        = FS_NULL;
    
    uint32_t proc = create_knode("proc", fs_current.current_directory);
    uint32_t dev  = create_knode("dev", fs_current.current_directory);
    uint32_t mnt  = create_knode("mnt", fs_current.current_directory);
    
    hardware_identify_devices(dev, mnt, dev);
    
    // Check mounted devices
    uint32_t device_mount = KMALLOC_NULL;
    for (uint16_t i=0;; i++) {
        uint32_t mount_point = knode_get_reference(mnt, i);
        uint32_t mount_device = knode_get_reference(mount_point, 0);
        
        if (mount_point == KMALLOC_NULL) 
            break;
        
        uint32_t reference = knode_get_reference(mount_point, 0);
        if (reference == KMALLOC_NULL) 
            continue;
        device_mount = reference;
        
        struct FSPartitionBlock part;
        fs_device_open(device_mount, &part);
        
        fs_current.current_directory = mount_point;
        fs_current.mount_device      = mount_device;
        fs_current.mount_directory   = part.root_directory;
        fs_current.mount_root        = part.root_directory;
        kernel_set_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
        
        char path_buf[32];
        console_get_path(path_buf, 32, fs_current.current_directory, fs_current.mount_directory, 16);
        strcpy(fs_current.path, path_buf);
        
        // Add directory to path
        strcpy(&fs_current.path[strlen(path_buf)], "/bin;");
        
        // Complete the prompt
        
        strcpy(&path_buf[strlen(path_buf)], ">\0");
        console_set_prompt(path_buf);
        break;
    }
    
    // Default to the root directory
    if (device_mount == KMALLOC_NULL) {
        fs_current.current_directory = root_node;
        fs_current.mount_device      = FS_NULL;
        fs_current.mount_directory   = FS_NULL;
        fs_current.mount_root        = FS_NULL;
        kernel_set_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
        
        console_set_prompt("/>");
        return;
    }
    
    kernel_set_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
}

uint32_t create_device(const char* name) {
    struct Device device;
    memset(&device, 0x00, sizeof(struct Device));
    strcpy(device.name, name);
    
    uint32_t device_address = kmalloc(sizeof(struct Device));
    
    kmalloc_set_type(device_address, KMALLOC_TYPE_DEVICE);
    kmalloc_set_permissions(device_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmalloc_write(device_address, &device, sizeof(struct Device));
    return device_address;
}

void destroy_device(uint32_t address) {
    kfree(address);
}

uint32_t create_buffer(uint32_t size) {
    uint8_t buffer[size];
    memset(&buffer, 0x00, size);
    
    uint32_t buffer_address = kmalloc(size);
    kmalloc_set_type(buffer_address, KMALLOC_TYPE_DEVICE);
    
    kmalloc_write(buffer_address, &buffer, size);
    return buffer_address;
}

void destroy_buffer(uint32_t address) {
    struct KMallocHeader header;
    kmalloc_read(address - sizeof(struct KMallocHeader), &header, sizeof(struct KMallocHeader));
    
    uint32_t size = header.size;
    uint8_t buffer[size];
    memset(&buffer, 0x00, size);
    
    kmalloc_write(address, &buffer, size);
    kfree(address);
}

uint32_t create_driver(const char* name) {
    struct Driver driver;
    memset(&driver, 0x00, sizeof(struct Driver));
    strcpy(driver.name, name);
    
    uint32_t driver_address = kmalloc(sizeof(struct Driver));
    
    kmalloc_set_type(driver_address, KMALLOC_TYPE_DRIVER);
    kmalloc_set_permissions(driver_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmalloc_write(driver_address, &driver, sizeof(struct Driver));
    return driver_address;
}

void destroy_driver(uint32_t address) {
    kfree(address);
}


void hardware_identify_devices(uint32_t knode_device, uint32_t knode_mount, uint32_t nt_device) {
    struct Bus bus;
    bus.read_waitstate  = 20;
    bus.write_waitstate = 20;
    
    uint16_t mount_count = 0;
    uint16_t nt_count = 0;
    
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
        
        // NT devices
        if (id == 0x14) {
            char device_name[] = "nic0";
            device_name[3] = nt_count + '0';
            nt_count++;
            
            //uint32_t block_address = create_buffer(128);
            
            uint32_t nt_ptr = create_device(device_name);
            
            knode_add_reference(nt_device, nt_ptr);
            continue;
        }
        
        // Mount file system devices
        if (id == 0x13) {
            char device_name[] = "ssd0";
            device_name[sizeof(device_name)-2] = (mount_count-1) + '0';
            
            uint32_t mount_ptr = create_knode(device_name, knode_mount);
            kmalloc_set_flags(mount_ptr, KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT);
            
            // Set the mount point address to the device base address
            knode_add_reference(mount_ptr, base);
            
            continue;
        }
        
        struct Device device;
        strcpy(device.name, name);
        
        device.bus.read_waitstate = 20;
        device.bus.write_waitstate = 20;
        
        device.id = id;
        device.class = 0;
        
        device.hardware_address = base;
        device.hardware_slot = (base - PERIPHERAL_ADDRESS_BEGIN) / PERIPHERAL_STRIDE + 1;
        
        uint32_t device_ptr = create_device(name);
        kmalloc_write(device_ptr, &device, sizeof(struct Device));
        
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
