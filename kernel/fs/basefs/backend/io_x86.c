#include <stdint.h>
#include <stdbool.h>

#include <kernel/arch/x86/drivers/ata/ata.h>
#include <kernel/arch/x86/drivers/ahci/ahci.h>
#include <kernel/fs/basefs/io.h>
#include <kernel/fs/basefs/structs.h>

static struct AHCI_Port_Registers* ahci_get_active_port(void) {
    for (int i = 0; i < 32; i++) {
        struct AHCI_Port_Registers* port = ahci_get_port(i); 
        if (port != NULL && port->signature == AHCI_DEV_SATA) { 
            return port;
        }
    }
    return NULL;
}

static bool fs_cache_flush_and_fetch(struct FSDeviceContext* ctx, uint32_t target_sector) {
    if (!ctx) return false; 
    if (ctx->sector_frame == target_sector) { 
        return true; 
    }
    
    uint8_t* window_ptr = (uint8_t*)(ctx->device_address); 
    if (ctx->device_type == FS_DEVICE_TYPE_ATA) { 
        if (ctx->sector_dirty && ctx->sector_frame != 0xFFFFFFFF) { 
            if (!ata_write_sector(ctx->sector_frame, window_ptr)) { 
                return false; 
            }
            ctx->sector_dirty = false; 
        }
        
        if (!ata_read_sector(target_sector, window_ptr)) { 
            return false; 
        }
        
        ctx->sector_frame = target_sector; 
    } 
    else if (ctx->device_type == FS_DEVICE_TYPE_AHCI) { 
        struct AHCI_Port_Registers* ahci_port = ahci_get_active_port();
        if (!ahci_port) return false;

        if (ctx->sector_dirty && ctx->sector_frame != 0xFFFFFFFF) { 
            if (!ahci_write_sectors(ahci_port, ctx->sector_frame, 1, window_ptr)) { 
                return false; 
            }
            ctx->sector_dirty = false; 
        }
        
        if (!ahci_read_sectors(ahci_port, target_sector, 1, window_ptr)) { 
            return false; 
        }
        
        ctx->sector_frame = target_sector; 
    }
    
    return true; 
}

void fs_cache_sync(struct FSDeviceContext* ctx) {
    if (!ctx) return; 
    if (ctx->device_type == FS_DEVICE_TYPE_ATA) { 
        if (ctx->sector_dirty && ctx->sector_frame != 0xFFFFFFFF) { 
            ata_write_sector(ctx->sector_frame, (uint8_t*)ctx->device_address); 
            ctx->sector_dirty = false; 
        }
    } 
    else if (ctx->device_type == FS_DEVICE_TYPE_AHCI) { 
        struct AHCI_Port_Registers* ahci_port = ahci_get_active_port();
        if (!ahci_port) return;

        if (ctx->sector_dirty && ctx->sector_frame != 0xFFFFFFFF) { 
            ahci_write_sectors(ahci_port, ctx->sector_frame, 1, (uint8_t*)ctx->device_address); 
            ctx->sector_dirty = false; 
        }
    }
}

uint8_t fs_readb(struct FSDeviceContext* ctx, uint32_t address) {
    if (!ctx) return 0;
    uint32_t target_sector = address / ATA_SECTOR_SIZE;
    uint32_t byte_offset = address % ATA_SECTOR_SIZE;
    
    if (!fs_cache_flush_and_fetch(ctx, target_sector)) {
        return 0;
    }
    
    return ((uint8_t*)ctx->device_address)[byte_offset];
}

void fs_writeb(struct FSDeviceContext* ctx, uint32_t address, uint8_t byte) {
    if (!ctx) return;
    uint32_t target_sector = address / ATA_SECTOR_SIZE;
    uint32_t byte_offset = address % ATA_SECTOR_SIZE;
    
    if (!fs_cache_flush_and_fetch(ctx, target_sector)) {
        return;
    }
    
    ((uint8_t*)ctx->device_address)[byte_offset] = byte;
    ctx->sector_dirty = true;
}

void fs_mem_read(struct FSDeviceContext* ctx, uint32_t address, void* destination, uint32_t size) {
    if (!ctx) return;
    uint8_t* dest_ptr = (uint8_t*)destination;
    for (uint32_t i = 0; i < size; i++) {
        dest_ptr[i] = fs_readb(ctx, address + i);
    }
}

void fs_mem_write(struct FSDeviceContext* ctx, uint32_t address, const void* source, uint32_t size) {
    if (!ctx) return;
    const uint8_t* src_ptr = (const uint8_t*)source;
    for (uint32_t i = 0; i < size; i++) {
        fs_writeb(ctx, address + i, src_ptr[i]);
    }
}
