#include <kernel/arch/x86/bus/pci.h>
#include <kernel/arch/x86/io.h>

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldevice = (uint32_t)device;
    uint32_t lfunc = (uint32_t)func;
    
    // Construct the 32-bit configuration address
    address = (uint32_t)((lbus << 16) | (ldevice << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
              
    // Write the address to the Config Address Port
    outl(0xCF8, address);
    
    // Read the data from the Config Data Port
    return inl(0xCFC);
}

// Mock print function—replace this with your kernel's actual VGA/Serial print function
void kprint_pci_info(uint8_t bus, uint8_t dev, uint8_t func, uint16_t vendor, uint16_t device, uint8_t class_code, uint8_t subclass) {
    // Inside your kernel, you'd format a string and output it to the screen or serial port.
    // Example format: "PCI [00:02.0] Vendor: 0x8086 Device: 0x10D3 Class: 0x01 Subclass: 0x06"
}

void pci_probe_bus(void) {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            // Check function 0 first
            uint32_t reg0 = pci_config_read(bus, dev, 0, 0);
            uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
            
            // If vendor ID is 0xFFFF or 0x0000, no device is present here
            if (vendor_id == 0xFFFF || vendor_id == 0x0000) 
                continue;
            
            // A device was found, Now loop through the 8 possible functions
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id_reg = pci_config_read(bus, dev, func, 0);
                uint16_t v_id = (uint16_t)(id_reg & 0xFFFF);
                
                if (v_id == 0xFFFF || v_id == 0x0000) {
                    continue; // Skip invalid functions
                }
                
                uint16_t d_id = (uint16_t)((id_reg >> 16) & 0xFFFF);
                
                // Read Class Code and Subclass (located at offset 0x08)
                // Register 0x08 format: 
                // Bits 31-24: Class, Bits 23-16: Subclass, Bits 15-8: Prog IF, Bits 7-0: Revision
                uint32_t class_reg = pci_config_read(bus, dev, func, 0x08);
                uint8_t class_code = (uint8_t)((class_reg >> 24) & 0xFF);
                uint8_t subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
                
                // Check if this is your SSD controller
                if (class_code == 0x01) { // Mass Storage Controller
                    if (subclass == 0x06) {
                        // Found an AHCI (SATA) Controller!
                    } else if (subclass == 0x08) {
                        // Found an NVMe Controller!
                    }
                }
                
                // Print device information
                kprint_pci_info(bus, dev, func, v_id, d_id, class_code, subclass);
            }
        }
    }
}
