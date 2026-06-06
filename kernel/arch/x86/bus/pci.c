#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/bus/pci.h>
#include <kernel/arch/x86/paging.h>
#include <kernel/arch/x86/page_alloc.h>

#include <kernel/arch/x86/drivers/ata.h>
#include <kernel/fs/fs.h>

#include <kernel/console/print.h>
#include <kernel/knode.h>
#include <kernel/util/string.h>

#define PAGE_SIZE 4096

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
        
#ifdef PCI_DEBUG_HARDWARE_DUMP
        
        if (original_bar != 0) {
            // Print out what the BAR actually contains
            if (original_bar & 0x1) {
                print("I/O  ::  ");
            } else {
                print("MMIO  ::  ");
            }
        }
        
#endif
        
    }
    
    for (uint8_t bar_index = 0; bar_index < 6; bar_index++) {
        uint8_t bar_offset = 0x10 + (bar_index * 4);
        uint32_t original_bar = pci_config_read(bus, dev, func, bar_offset);
        
        if (original_bar == 0) continue;
        
        if ((original_bar & 0x1) == 0) { // MMIO Space
            uint32_t physical_mmio_addr = original_bar & 0xFFFFFFF0;
            if (physical_mmio_addr == 0) 
                continue;
            
            pci_config_write(bus, dev, func, bar_offset, 0xFFFFFFFF);
            uint32_t size_mask = pci_config_read(bus, dev, func, bar_offset);
            pci_config_write(bus, dev, func, bar_offset, original_bar);
            
            size_mask &= 0xFFFFFFF0;
            if (size_mask == 0) continue;
            
            uint32_t bar_size_bytes = ~size_mask + 1;
            size_t num_pages = (bar_size_bytes + (PAGE_SIZE - 1)) / PAGE_SIZE;
            
            void* virt_addr = alloc_virt_mmio(physical_mmio_addr, num_pages);
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

void pci_scan_bus(uint8_t bus_number, uint32_t pci_directory, uint32_t mnt_directory) {
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
            if (v_id == 0xFFFF || v_id == 0x0000) {
                continue; 
            }
            
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
            
            void* base_virtual_address = pci_map_device_bars(bus_number, dev, func);
            
            char device_node_name[16];
            format_device_string(device_node_name, dev, func);
            
            uint32_t device_knode = create_knode(device_node_name, pci_directory);
            
            const char* type_name = pci_get_class_name(class_code);
            uint32_t type_knode = create_knode(type_name, device_knode);
            
            
#ifdef PCI_DEBUG_HARDWARE_DUMP
            
            char base_address[16];
            format_hex32_string(base_address, (uint32_t)base_virtual_address);
            
            print( type_name );
            print(": ");
            print( base_address );
            print("\n");
            
#endif
            
            //
            // Initiate ATA devices
            
            if (class_code == 0x01 && subclass == 0x01) { // Storage Controller, IDE
                uint16_t primary_io_base = 0;
                
                // Check Prog IF to see if it's Native Mode or Legacy Compatibility Mode
                if (prog_if & 0x01) {
                    // Native Mode: Read BAR 0 for Primary Channel Command Ports
                    primary_io_base = pci_get_io_bar(bus_number, dev, func, 0);
                } else {
                    // Legacy Mode: Force the standard PC architecture ports
                    primary_io_base = 0x1F0;
                }
                
                if (primary_io_base != 0) {
                    bool ata_present = ata_init(primary_io_base);
                    if (ata_present) {
                        
                        {
                        //
                        // Quick format example
                        /*
                        uint32_t device_size = 1024 * 1024 * 32;
                        
                        uint32_t device_address = kmalloc( 512 );
                        kmemset(device_address, 0x00, 512);
                        
                        fs_device_format(device_address, device_size, 512);
                        
                        struct FSPartitionBlock part;
                        fs_device_open(device_address, &part);
                        
                        uint32_t root_directory = fs_directory_create("root", FS_PERMISSION_READ | FS_PERMISSION_WRITE, FS_NULL);
                        
                        part.root_directory = root_directory;
                        
                        fs_mem_write(sizeof(struct FSDeviceHeader), &part, sizeof(struct FSPartitionBlock));
                        
                        fs_cache_sync();
                        */
                        }
                        
                        
                        
                        char device_name[] = "ssd ";
                        device_name[3] = '0' + storage_device_index++;
                        
                        uint32_t mount_ptr = create_knode(device_name, mnt_directory);
                        kmalloc_set_flags(mount_ptr, KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT);
                        
                        uint32_t block_device = kmalloc( 512 );
                        
                        struct FSPartitionBlock part;
                        fs_device_open(block_device, &part);
                        
                        ata_read_sector(0, (uint8_t*)block_device);
                        
                        knode_add_reference(mount_ptr, block_device);
                        
                        
                        
                        
                        
                    }
                }
            }
            
            
            
            
            char value_buffer[16];
            uint32_t prop_dir;
            
            prop_dir = create_knode("bar", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex32_string(value_buffer, (uint32_t)base_virtual_address);
            create_knode(value_buffer, prop_dir);
            
            prop_dir = create_knode("vendor", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex16_string(value_buffer, v_id);
            create_knode(value_buffer, prop_dir);
            
            prop_dir = create_knode("device", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_hex16_string(value_buffer, d_id);
            create_knode(value_buffer, prop_dir);
            
            prop_dir = create_knode("class", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_dec8_string(value_buffer, class_code);
            create_knode(value_buffer, prop_dir);
            
            prop_dir = create_knode("subclass", type_knode);
            memset(value_buffer, 0, sizeof(value_buffer));
            format_dec8_string(value_buffer, subclass);
            create_knode(value_buffer, prop_dir);
            
            // Subsystem Properties (Only if valid/assigned)
            if (sub_v_id != 0x0000 && sub_v_id != 0xFFFF) {
                prop_dir = create_knode("sub_vendor", type_knode);
                format_hex16_string(value_buffer, sub_v_id);
                create_knode(value_buffer, prop_dir);
                
                prop_dir = create_knode("sub_id", type_knode);
                format_hex16_string(value_buffer, sub_id);
                create_knode(value_buffer, prop_dir);
            }
            
            // Interrupt Pin & Line Configuration Context
            if (irq_pin > 0 && irq_pin <= 4) {
                prop_dir = create_knode("pin", type_knode);
                format_dec8_string(value_buffer, irq_pin);
                create_knode(value_buffer, prop_dir);
                
                if (irq_line != 0xFF) {
                    prop_dir = create_knode("irq", type_knode);
                    format_dec8_string(value_buffer, irq_line);
                    create_knode(value_buffer, prop_dir);
                }
            }
            
            // Identify and handle PCI-to-PCI Bridge devices internally via recursion
            if (class_code == 0x06 && subclass == 0x04) {
                // Register 0x18 details: Primary, Secondary, and Subordinate bus assignments
                uint32_t bus_reg = pci_config_read(bus_number, dev, func, 0x18);
                uint8_t secondary_bus = (uint8_t)((bus_reg >> 8) & 0xFF);
                
                char sub_bus_name[16];
                format_bus_string(sub_bus_name, secondary_bus);
                
                uint32_t secondary_bus_directory = create_knode(sub_bus_name, type_knode);
                
                pci_scan_bus(secondary_bus, secondary_bus_directory, mnt_directory);
            }
            
        }
    }
}

void pci_init(void) {
    uint32_t root_node = knode_get_root();
    uint32_t dev_directory = knode_find_by_name(root_node, "dev");
    uint32_t mnt_directory = knode_find_by_name(root_node, "mnt");
    uint32_t pci_directory = create_knode("pci", dev_directory);
    
    uint32_t bus0_directory = create_knode("bus0", pci_directory);
    
    pci_scan_bus(0, bus0_directory, mnt_directory);
}

