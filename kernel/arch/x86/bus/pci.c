#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/bus/pci.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/arch/x86/drivers/ahci/ahci.h>
#include <kernel/arch/x86/drivers/ata/ata.h>

#include <kernel/kernel.h>
#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/console/print.h>
#include <kernel/util/string.h>

//#define PCI_DEBUG_HARDWARE_DUMP

uint16_t storage_device_index = 0;

static void format_device_string(char* buf, uint8_t dev, uint8_t func) {
    buf[0] = '0' + (dev / 10);
    buf[1] = '0' + (dev % 10);
    buf[2] = ':';
    buf[3] = '0' + (func / 10);
    buf[4] = '0' + (func % 10);
    buf[5] = '\0';
}

static void format_bus_string(char* buf, uint8_t bus) {
    buf[0] = 'b'; buf[1] = 'u'; buf[2] = 's';
    if (bus < 10) {
        buf[3] = '0' + bus;
        buf[4] = '\0';
    } else {
        buf[3] = '0' + (bus / 10);
        buf[4] = '0' + (bus % 10);
        buf[5] = '\0';
    }
}

static void format_hex32_string(char* buf, uint32_t val) {
    buf[0] = '0';
    buf[1] = 'x';
    u32tox(val, &buf[2]);
}

static void format_hex16_string(char* buf, uint16_t val) {
    buf[0] = '0';
    buf[1] = 'x';
    u16tox(val, &buf[2]);
}

static void format_hex8_string(char* buf, uint8_t val) {
    buf[0] = '0';
    buf[1] = 'x';
    u8tox(val, &buf[2]);
}

static void format_dec8_string(char* buf, uint8_t val) {
    size_t len = 0;
    if (val >= 100) {
        buf[len++] = '0' + (val / 100);
        buf[len++] = '0' + ((val % 100) / 10);
        buf[len++] = '0' + (val % 10);
    } else if (val >= 10) {
        buf[len++] = '0' + (val / 10);
        buf[len++] = '0' + (val % 10);
    } else {
        buf[len++] = '0' + val;
    }
    buf[len] = '\0';
}

const char* pci_get_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "legacy";
        case 0x01: return "storage";
        case 0x02: return "network";
        case 0x03: return "display";
        case 0x04: return "multimedia";
        case 0x06: return "bridge";
        case 0x0C: return "serialusb";
        default:   return "unknown";
    }
}

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                      ((uint32_t)func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                      ((uint32_t)func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outl(0xCFC, value);
}

void* pci_map_device_bars(uint8_t bus, uint8_t dev, uint8_t func) {
    void* first_mapped_vaddr = NULL;
    
    for (uint8_t bar_index = 0; bar_index < 6; bar_index++) {
        uint8_t bar_offset = 0x10 + (bar_index * 4);
        uint32_t original_bar = pci_config_read(bus, dev, func, bar_offset);
        
        if (original_bar == 0) continue;
        
        // Check if Bit 0 is 0 (MMIO Space)
        if ((original_bar & 0x01) == 0) { 
            uint32_t physical_mmio_addr = original_bar & 0xFFFFFFF0;
            if (physical_mmio_addr == 0) 
                continue;
            
            // Determine the allocation size required by the hardware BAR
            pci_config_write(bus, dev, func, bar_offset, 0xFFFFFFFF);
            uint32_t size_mask = pci_config_read(bus, dev, func, bar_offset);
            pci_config_write(bus, dev, func, bar_offset, original_bar); // Restore original value
            
            size_mask &= 0xFFFFFFF0;
            if (size_mask == 0) continue;
            
            uint32_t bar_size_bytes = ~size_mask + 1;
            
            // Map via VMM using Cache-Disable (VM_PCD) to prevent stale registers
            uint32_t mmio_flags = VM_PRESENT | VM_READWRITE | VM_PCD;
            void* virt_addr = vmm_map_mmio_region(physical_mmio_addr, bar_size_bytes, mmio_flags);
            
            // Track the first successfully mapped virtual address to return
            if (virt_addr != NULL && first_mapped_vaddr == NULL) {
                first_mapped_vaddr = virt_addr; 
            }
        }
    }
    return first_mapped_vaddr;
}

uint16_t pci_get_io_bar(uint8_t bus, uint8_t dev, uint8_t func, uint8_t bar_index) {
    uint8_t bar_offset = 0x10 + (bar_index * 4);
    uint32_t bar = pci_config_read(bus, dev, func, bar_offset);
    
    // Check if it is a valid, populated I/O BAR
    if (bar != 0 && (bar & 0x01) == 1) {
        // Bit 0 and 1 are formatting flags; mask them out
        return (uint16_t)(bar & 0xFFFFFFFC);
    }
    
    return 0; // Not an I/O BAR or empty
}


void ahci_test_write(struct AHCI_Port_Registers* active_port) {
    if (active_port == NULL) {
        print("AHCI Test Error: active_port is NULL\n");
        return;
    }
    
    print("--- Starting AHCI Hex Editor Verification Test ---\n");
    
    // 1. Allocate a 512-byte temporary block in memory
    uint8_t* test_buffer = (uint8_t*)malloc(512);
    if (test_buffer == NULL) {
        print("AHCI Test Error: Failed to allocate test buffer memory.\n");
        return;
    }
    
    // 2. Clear the buffer entirely
    memset(test_buffer, 0, 512);
    
    // 3. Bake a highly distinct string pattern into the buffer
    const char* magic_string = "DEADBEEF CHICKEN NUGGET TEST PATTERN - QEMU VIRTUAL DISK WRITE OK!";
    size_t str_len = strlen(magic_string);
    memcpy(test_buffer, magic_string, str_len);
    
    // Fill the tail end of the sector with recognizable incrementing hex bytes
    for (int i = 128; i < 512; i++) {
        test_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    print("Attempting to write test pattern to LBA Sector 1...\n");
    
    // 4. Issue the write transaction to LBA 1 (1 sector count)
    // Using LBA 1 preserves Sector 0 just in case you have structural data there
    if (ahci_write_sectors(active_port, 1, 1, test_buffer)) {
        print("SUCCESS: Sector 1 written successfully!\n");
        print("Action Required: Close QEMU and open your disk image in a hex editor.\n");
        print("Look at file offset 0x200 (512 bytes in) to find your string!\n");
    } else {
        print("FAILURE: AHCI controller rejected or timed out on the write verification transfer.\n");
    }
    
    // 5. Clean up allocated resources
    free(test_buffer);
    print("--------------------------------------------------\n");
}

void pci_scan_bus(uint8_t bus_number, uint32_t pci_directory, uint32_t mnt_directory) {
    struct LocalPaths fs_paths;
    kernel_get_local_paths(&fs_paths);
    
    for (uint8_t dev = 0; dev < 32; dev++) {
        // Read Register 0 (Vendor/Device ID) for Function 0 first
        uint32_t reg0 = pci_config_read(bus_number, dev, 0, 0);
        uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
        
        // If no device exists on Function 0, skip this slot entirely
        if (vendor_id == 0xFFFF || vendor_id == 0x0000) 
            continue;
        
        // Read the Header Type register at offset 0x0E (contained within doubleword 0x0C)
        uint32_t header_reg = pci_config_read(bus_number, dev, 0, 0x0C);
        uint8_t header_type = (uint8_t)((header_reg >> 16) & 0xFF);
        
        // Bit 7 of Header Type indicates whether the device handles multiple functions
        uint8_t max_functions = (header_type & 0x80) ? 8 : 1;
        
        for (uint8_t func = 0; func < max_functions; func++) {
            uint32_t id_reg = pci_config_read(bus_number, dev, func, 0);
            uint16_t v_id = (uint16_t)(id_reg & 0xFFFF);
            uint16_t d_id = (uint16_t)((id_reg >> 16) & 0xFFFF);
            
            // Validate individual function presence
            if (v_id == 0xFFFF || v_id == 0x0000) 
                continue;
            
            // Read Class, Subclass, and Programming Interface (Prog IF) identifiers at offset 0x08
            uint32_t class_reg = pci_config_read(bus_number, dev, func, 0x08);
            uint8_t class_code = (uint8_t)((class_reg >> 24) & 0xFF);
            uint8_t subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
            uint8_t prog_if    = (uint8_t)((class_reg >> 8) & 0xFF);
            
            // Read Subsystem ID and Subsystem Vendor ID at offset 0x2C
            uint32_t sub_reg   = pci_config_read(bus_number, dev, func, 0x2C);
            uint16_t sub_v_id  = (uint16_t)(sub_reg & 0xFFFF);
            uint16_t sub_id    = (uint16_t)((sub_reg >> 16) & 0xFFFF);
            
            // Read Interrupt Line and Interrupt Pin configuration mappings at offset 0x3C
            uint32_t intr_reg  = pci_config_read(bus_number, dev, func, 0x3C);
            uint8_t irq_line   = (uint8_t)(intr_reg & 0xFF);
            uint8_t irq_pin    = (uint8_t)((intr_reg >> 8) & 0xFF);
            
            // Dynamic allocation map for BARs via VMM
            void* base_virtual_address = pci_map_device_bars(bus_number, dev, func);
            
            char device_node_name[16];
            format_device_string(device_node_name, dev, func);
            
            uint32_t device_knode = create_knode(device_node_name, pci_directory);
            
            const char* type_name = pci_get_class_name(class_code);
            uint32_t type_knode = create_knode(type_name, device_knode);
            
            knode_set_permissions(device_knode, KMALLOC_PERMISSION_READ);
            knode_set_permissions(type_knode, KMALLOC_PERMISSION_READ);
            
#ifdef PCI_DEBUG_HARDWARE_DUMP
            char base_address[16];
            format_hex32_string(base_address, (uint32_t)base_virtual_address);
            
            print(type_name);
            print(": ");
            print(base_address);
            print("\n");
#endif
            
            //
            // Handle Storage Devices
            
            if (class_code == 0x01) { 
                
                uint32_t pci_cmd = pci_config_read(bus_number, dev, func, 0x04);
                pci_cmd |= (1 << 1) | (1 << 2); 
                pci_config_write(bus_number, dev, func, 0x04, pci_cmd);
                
                // Legacy ATA (IDE) support
                uint32_t target_device = FS_NULL;
                
                if (subclass == 0x01) {
                    uint16_t primary_io_base = 0;
                    
                    if (prog_if & 0x01) {
                        primary_io_base = pci_get_io_bar(bus_number, dev, func, 0);
                    } else {
                        primary_io_base = 0x1F0;
                    }
                    
                    if (primary_io_base != 0) {
                        bool ata_present = ata_init(primary_io_base);
                        if (ata_present) {
                            char device_name[] = "ata \0";
                            device_name[3] = '0' + storage_device_index++;
                            
                            // Mount the file system into the knode structure
                            
                            uint32_t mount_ptr = create_knode(device_name, mnt_directory);
                            kmalloc_set_flags(mount_ptr, (KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT));
                            
                            uint32_t block_device = (uint32_t)malloc(512);
                            target_device = block_device;
                            
                            struct FSPartitionBlock part;
                            struct FSDeviceContext context = fs_device_open(block_device, &part, FS_DEVICE_TYPE_ATA);
                            
                            uint32_t device_context = (uint32_t)malloc(sizeof(struct FSDeviceContext));
                            memcpy((void*)device_context, &context, sizeof(struct FSDeviceContext));
                            
                            ata_read_sector(0, (uint8_t*)block_device);
                            knode_add_reference(mount_ptr, block_device);
                            knode_add_reference(mount_ptr, device_context);
                            
                            // Set local paths
                            
                            char mount_root[64];
                            memset(mount_root, '\0', sizeof(mount_root));
                            strcat(mount_root, "/mnt/");
                            strncat(mount_root, device_name, 64);
                            
                            // Check ata device directories
                            
                            // Check bin directory
                            char bin_path[64];
                            memset(bin_path, '\0', sizeof(bin_path));
                            strncpy(bin_path, mount_root, 64);
                            strcat(bin_path, "/bin");
                            
                            if (vfs_directory_check(bin_path)) {
                                strcat(bin_path, ";");
                                strncpy(fs_paths.path, bin_path, PATH_LENGTH_MAX);
                            }
                            
                            // Check sys directory
                            
                            char sys_path[64];
                            memset(sys_path, '\0', sizeof(sys_path));
                            strncpy(sys_path, mount_root, 64);
                            strcat(sys_path, "/sys");
                            
                            if (vfs_directory_check(sys_path)) {
                                strncpy(fs_paths.home, sys_path, PATH_LENGTH_MAX);
                            }
                            
                            print("ATA device mounted\n");
                            
                        }
                    }
                } 
                // AHCI (SATA) Controller section inside pci_scan_bus
                else if (subclass == 0x06) { 
                    // BAR 5 contains the physical ABAR
                    uint32_t abar_phys = pci_config_read(bus_number, dev, func, 0x24); 
                    if (abar_phys != 0 && (abar_phys & 0x01) == 0) { 
                        
                        // Enable PCI bus mastering (bit 2) and mmio space (bit 1)
                        uint32_t pci_cmd = pci_config_read(bus_number, dev, func, 0x04); 
                        pci_cmd |= (1 << 2) | (1 << 1); 
                        pci_config_write(bus_number, dev, func, 0x04, pci_cmd); 
                        
                        uint32_t phys_mmio_addr = abar_phys & 0xFFFFFFF0; 
                        uint32_t flags =  VM_PRESENT | VM_READWRITE | VM_PCD; 
                        
                        struct AHCI_HBA_Memory_Space* ahci_base_vaddr = (struct AHCI_HBA_Memory_Space*)vmm_map_mmio_region(phys_mmio_addr, 4096, flags); 
                        if (ahci_base_vaddr != NULL) { 
                            ahci_init(ahci_base_vaddr); 
                            
                            // Find which port has the drive we initialized in ahci_init
                            struct AHCI_Port_Registers* active_port = NULL; 
                            for (int p = 0; p < 32; p++) { 
                                if (ahci_base_vaddr->ports_implemented & (1 << p)) { 
                                    if (ahci_base_vaddr->ports[p].signature == AHCI_DEV_SATA) { 
                                        active_port = &ahci_base_vaddr->ports[p]; 
                                        break; 
                                    }
                                }
                            }
                            
                            if (active_port != NULL) { 
                                char device_name[] = "ahci ";
                                device_name[4] = '0' + storage_device_index;
                                
                                uint32_t block_device = (uint32_t)vmm_alloc_pages(1);
                                if (block_device == 0) {
                                    print("AHCI Error: Failed to allocate page-aligned block device buffer.\n");
                                    return;
                                }
                                
                                // Read LBA sector 0
                                if (ahci_read_sectors(active_port, 0, 1, (uint8_t*)block_device)) { 
                                    uint32_t mount_ptr = create_knode(device_name, mnt_directory); 
                                    kmalloc_set_flags(mount_ptr, (KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT)); 
                                    
                                    struct FSPartitionBlock part; 
                                    struct FSDeviceContext context = fs_device_open(block_device, &part, FS_DEVICE_TYPE_AHCI); 
                                    
                                    uint32_t device_context = (uint32_t)malloc(sizeof(struct FSDeviceContext)); 
                                    memcpy((void*)device_context, &context, sizeof(struct FSDeviceContext)); 
                                    
                                    knode_add_reference(mount_ptr, block_device); 
                                    knode_add_reference(mount_ptr, device_context); 
                                    
                                    storage_device_index++; 
                                    print("AHCI device mounted\n"); 
                                } else {
                                    print("AHCI error - failed to read sector 0\n"); 
                                    vmm_free_pages((void*)block_device, 1);
                                }
                            }
                        }
                    }
                }
            }
            // Standard virtual VFS property nodes generation
            char value_buffer[16];
            uint32_t prop_dir;
            
            prop_dir = create_knode("bar", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex32_string(value_buffer, (uint32_t)base_virtual_address);
            uint32_t device_bar = create_device(value_buffer);
            knode_add_reference(prop_dir, device_bar);
            knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
            knode_set_permissions(device_bar, KMALLOC_PERMISSION_READ);
            
            prop_dir = create_knode("vendor", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex16_string(value_buffer, v_id);
            uint32_t device_vendor = create_device(value_buffer);
            knode_add_reference(prop_dir, device_vendor);
            knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
            knode_set_permissions(device_vendor, KMALLOC_PERMISSION_READ);
            
            prop_dir = create_knode("device", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex16_string(value_buffer, d_id);
            uint32_t device_dev = create_device(value_buffer);
            knode_add_reference(prop_dir, device_dev);
            knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
            knode_set_permissions(device_dev, KMALLOC_PERMISSION_READ);
            
            prop_dir = create_knode("class", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_dec8_string(value_buffer, class_code);
            uint32_t device_class = create_device(value_buffer);
            knode_add_reference(prop_dir, device_class);
            knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
            knode_set_permissions(device_class, KMALLOC_PERMISSION_READ);
            
            prop_dir = create_knode("subclass", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_dec8_string(value_buffer, subclass);
            uint32_t device_subclass = create_device(value_buffer);
            knode_add_reference(prop_dir, device_subclass);
            knode_set_permissions(device_subclass, KMALLOC_PERMISSION_READ);
            
            if (sub_v_id != 0x0000 && sub_v_id != 0xFFFF) {
                prop_dir = create_knode("sub_vendor", type_knode);
                format_hex16_string(value_buffer, sub_v_id);
                uint32_t sub_vendor = create_device(value_buffer);
                knode_add_reference(prop_dir, sub_vendor);
                knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
                knode_set_permissions(sub_vendor, KMALLOC_PERMISSION_READ);
                
                prop_dir = create_knode("sub_id", type_knode);
                format_hex16_string(value_buffer, sub_id);
                uint32_t sub_id_address = create_device(value_buffer);
                knode_add_reference(prop_dir, sub_id_address);
                knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
                knode_set_permissions(sub_id_address, KMALLOC_PERMISSION_READ);
            }
            
            if (irq_pin > 0 && irq_pin <= 4) {
                prop_dir = create_knode("pin", type_knode);
                format_dec8_string(value_buffer, irq_pin);
                uint32_t device_address = create_device(value_buffer);
                knode_add_reference(prop_dir, device_address);
                knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
                knode_set_permissions(device_address, KMALLOC_PERMISSION_READ);
                
                if (irq_line != 0xFF) {
                    prop_dir = create_knode("irq", type_knode);
                    format_dec8_string(value_buffer, irq_line);
                    uint32_t device_address = create_device(value_buffer);
                    knode_add_reference(prop_dir, device_address);
                    knode_set_permissions(prop_dir, KMALLOC_PERMISSION_READ);
                    knode_set_permissions(device_address, KMALLOC_PERMISSION_READ);
                }
            }
            
            // Handle PCI-to-PCI Bridges
            if (class_code == 0x06 && subclass == 0x04) {
                uint32_t bus_reg = pci_config_read(bus_number, dev, func, 0x18);
                uint8_t secondary_bus = (uint8_t)((bus_reg >> 8) & 0xFF);
                
                char sub_bus_name[16];
                format_bus_string(sub_bus_name, secondary_bus);
                
                uint32_t secondary_bus_directory = create_knode(sub_bus_name, type_knode);
                knode_set_permissions(secondary_bus_directory, KMALLOC_PERMISSION_READ);
                
                pci_scan_bus(secondary_bus, secondary_bus_directory, mnt_directory);
            }
        }
    }
    
    // Finalize paths
    kernel_set_local_paths(&fs_paths);
}

void pci_init(void) {
    uint32_t root_node = knode_get_root();
    uint32_t dev_directory = knode_find_by_name(root_node, "dev");
    uint32_t mnt_directory = knode_find_by_name(root_node, "mnt");
    uint32_t pci_directory = create_knode("pci", dev_directory);
    
    uint32_t bus0_directory = create_knode("bus0", pci_directory);
    
    knode_set_permissions(pci_directory, KMALLOC_PERMISSION_READ);
    knode_set_permissions(bus0_directory, KMALLOC_PERMISSION_READ);
    
    pci_scan_bus(0, bus0_directory, mnt_directory);
}
