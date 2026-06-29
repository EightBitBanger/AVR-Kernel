#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/fs/basefs/io.h>
#include <kernel/arch/x86/drivers/ata/ata.h>
#include <kernel/arch/x86/drivers/ahci/ahci.h>

extern uint32_t fs_device_address;

extern uint16_t fs_device_type;

uint32_t fs_sector_frame = 0xFFFFFFFF;

static bool cache_dirty = false;

bool fs_cache_flush_and_fetch(uint32_t target_sector) {
    // If the requested sector is already loaded in the memory window, do nothing
    if (fs_sector_frame == target_sector) {
        return true;
    }
    
    // Cast the device destination address to a usable byte pointer
    uint8_t* window_ptr = (uint8_t*)fs_device_address;
    
    // Legacy ATA
    
    if (fs_device_type == FS_DEVICE_TYPE_ATA) {
        // Write-back check
        if (cache_dirty && fs_sector_frame != 0xFFFFFFFF) {
            if (!ata_write_sector(fs_sector_frame, window_ptr)) {
                return false;
            }
            cache_dirty = false;
        }
        
        // Read the new target sector directly into the memory window
        if (!ata_read_sector(target_sector, window_ptr)) {
            return false;
        }
        
        // Update tracking variables
        fs_sector_frame = target_sector;
    } 
    
    // AHCI
    
    else if (fs_device_type == FS_DEVICE_TYPE_AHCI) {
        struct AHCI_Port_Registers* ahci_port = ahci_get_port(0);
        
        // Write-back check
        if (cache_dirty && fs_sector_frame != 0xFFFFFFFF) {
            if (!ahci_write_sectors(ahci_port, target_sector, 1, window_ptr)) {
                return false;
            }
            cache_dirty = false;
        }
        
        // Read the new target sector directly into the memory window
        if (!ahci_read_sectors(ahci_port, target_sector, 1, window_ptr)) {
            return false;
        }
        
        // Update tracking variables
        fs_sector_frame = target_sector;
    }
    
    return true;
}

uint8_t fs_readb(uint32_t address) {
    uint32_t target_sector = address / ATA_SECTOR_SIZE;
    uint32_t byte_offset = address % ATA_SECTOR_SIZE;
    
    // Ensure the required sector is loaded into the fs_device_address window
    if (!fs_cache_flush_and_fetch(target_sector)) {
        return 0; 
    }
    
    // Read directly from the memory window at the calculated offset
    return ((uint8_t*)fs_device_address)[byte_offset];
}

void fs_writeb(uint32_t address, uint8_t byte) {
    uint32_t target_sector = address / ATA_SECTOR_SIZE;
    uint32_t byte_offset = address % ATA_SECTOR_SIZE;
    
    // Ensure the target sector is loaded into the window before making modifications
    if (!fs_cache_flush_and_fetch(target_sector)) {
        return; 
    }
    
    // Write the byte directly into the memory window at the offset
    ((uint8_t*)fs_device_address)[byte_offset] = byte;
    
    // Mark the window as dirty so it gets flushed when a sector swap occurs
    cache_dirty = true;
}

void fs_mem_read(uint32_t address, void* destination, uint32_t size) {
    uint8_t* dest_ptr = (uint8_t*)destination;
    for (uint32_t i = 0; i < size; i++) {
        dest_ptr[i] = fs_readb(address + i);
    }
}

void fs_mem_write(uint32_t address, const void* source, uint32_t size) {
    const uint8_t* src_ptr = (const uint8_t*)source;
    for (uint32_t i = 0; i < size; i++) {
        fs_writeb(address + i, src_ptr[i]);
    }
}

void fs_cache_sync(void) {
    
    // Legacy ATA
    
    if (fs_device_type == FS_DEVICE_TYPE_ATA) {
        if (cache_dirty && fs_sector_frame != 0xFFFFFFFF) {
            ata_write_sector(fs_sector_frame, (uint8_t*)fs_device_address);
            cache_dirty = false;
        }
    } 
    
    // AHCI
    
    else if (fs_device_type == FS_DEVICE_TYPE_AHCI) {
        struct AHCI_Port_Registers* ahci_port = ahci_get_port(0);
        
        if (cache_dirty && fs_sector_frame != 0xFFFFFFFF) {
            ahci_write_sectors(ahci_port,  fs_sector_frame, 1, (uint8_t*)fs_device_address);
            cache_dirty = false;
        }
    }
}
