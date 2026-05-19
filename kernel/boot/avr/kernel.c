#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <kernel/kernel.h>

#include <kernel/emulation/x4/scheduler.h>
#include <kernel/emulation/x4/x4.h>

#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>
#include <kernel/arch/avr/heap.h>

#include <kernel/bus/bus.h>

extern uint32_t k_system_object;


void kernel_init(void) {
    k_system_object = kmalloc(1024 * 4);
    
    uint32_t root_node = create_knode("", 0);
    knode_set_root(root_node);
    
    struct WorkingDirectory fs_current;
    struct LocalPaths fs_paths;
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
        kernel_set_working_directory(&fs_current);
        
        char path_buf[64];
        console_get_path(path_buf, 64, fs_current.current_directory, fs_current.mount_directory, 16);
        memcpy(fs_paths.path, path_buf, 64);
        
        // Add directory to path
        strcpy(&fs_paths.path[strlen(path_buf)], "/bin");
        
        // Complete the prompt
        strcpy(&path_buf[strlen(path_buf)], ">");
        console_prompt_set_string(path_buf);
        break;
    }
    
    // Default to the root directory
    if (device_mount == KMALLOC_NULL) {
        fs_current.current_directory = root_node;
        fs_current.mount_device      = FS_NULL;
        fs_current.mount_directory   = FS_NULL;
        fs_current.mount_root        = FS_NULL;
        kernel_set_working_directory(&fs_current);
        
        char* promptStr = "/>";
        console_prompt_set_string(promptStr);
        return;
    }
    
    kernel_set_local_paths(&fs_paths);
    kernel_set_working_directory(&fs_current);
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

void device_get_hardware_data(uint32_t address, char* data_buffer, uint8_t size) {
    char device_name[size];
    
    struct Bus bus;
    bus.read_waitstate  = 20;
    bus.write_waitstate = 20;
    
    for (uint8_t index=0; index < size; index++) 
        mmio_readb(&bus, address + index , &data_buffer[index]);
}
