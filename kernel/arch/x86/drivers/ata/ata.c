#include <kernel/arch/x86/drivers/ata/ata.h>
#include <kernel/arch/x86/io.h>
#include <kernel/util/string.h>

uint16_t io_base_address =0;

static inline bool ata_wait_ready(uint16_t io_base, uint8_t mask, uint8_t value, uint32_t timeout) {
    for (uint32_t i = 0; i < timeout; i++) {
        uint8_t status = inb(io_base_address + 7);
        // Ensure BUSY (0x80) is clear, and our target condition matches
        if (!(status & 0x80) && ((status & mask) == value)) {
            return true;
        }
    }
    return false;
}

bool ata_init(uint16_t io_base) {
    // Soft reset the device to ensure it's responsive
    outb(io_base + 6, 0xE0); // Select Master Drive, LBA mode
    
    // An optional short delay to let the controller settle
    for(volatile int i = 0; i < 1000; i++); 
    
    uint8_t status = inb(io_base + 7);
    if (status == 0xFF) {
        // 0xFF means nothing is connected to this I/O base port
        return false; 
    }
    
    io_base_address = io_base;
    return true;
}

bool ata_read_sector(uint32_t address, uint8_t* buffer) {
    uint16_t* word_buf = (uint16_t*)buffer;
    
    // Send Drive and Bits 24-27 of LBA
    outb(io_base_address + 6, 0xE0 | ((address >> 24) & 0x0F));
    
    // Sector count (1 sector)
    outb(io_base_address + 2, 1);
    
    // Send remaining LBA bytes
    outb(io_base_address + 3, (uint8_t)address);         // LBA Low (0-7)
    outb(io_base_address + 4, (uint8_t)(address >> 8));  // LBA Mid (8-15)
    outb(io_base_address + 5, (uint8_t)(address >> 16)); // LBA High (16-23)
    
    // Send read command (0x20)
    outb(io_base_address + 7, 0x20);
    
    // Wait until Data Request bit (0x08) is set
    if (!ata_wait_ready(io_base_address, 0x08, 0x08, 100000)) {
        return false; // Timeout or disk error
    }
    
    // Read 256 words (512 bytes total) from the Data Port
    for (int i = 0; i < 256; i++) {
        word_buf[i] = inw(io_base_address + 0);
    }
    
    return true;
}

bool ata_write_sector(uint32_t address, const uint8_t* buffer) {
    const uint16_t* word_buf = (const uint16_t*)buffer;
    
    // Send drive and bits 24-27 of LBA
    outb(io_base_address + 6, 0xE0 | ((address >> 24) & 0x0F));
    
    // Sector count (1 sector)
    outb(io_base_address + 2, 1);
    
    // Send remaining LBA bytes
    outb(io_base_address + 3, (uint8_t)address);
    outb(io_base_address + 4, (uint8_t)(address >> 8));
    outb(io_base_address + 5, (uint8_t)(address >> 16));
    
    // Send Write Command (0x30)
    outb(io_base_address + 7, 0x30);
    
    // Wait until drive clears BUSY and sets Data Request (0x08), ready to consume data
    if (!ata_wait_ready(io_base_address, 0x08, 0x08, 100000)) {
        return false;
    }
    
    // Stream the 256 words (512 bytes) *out* to the Data Port
    for (int i = 0; i < 256; i++) {
        outw(io_base_address + 0, word_buf[i]);
    }
    
    // Send an ATA Flush Cache command (0xE7) to ensure physical commit to storage
    outb(io_base_address + 7, 0xE7);
    ata_wait_ready(io_base_address, 0x00, 0x00, 100000); // Wait for processing to stop
    return true;
}

bool ata_read_bytes(uint32_t address, uint8_t* buffer, size_t total_bytes) {
    if (buffer == NULL || total_bytes == 0) {
        return false;
    }
    
    // Calculate how many 512-byte sectors we need to read
    size_t total_sectors = total_bytes / ATA_SECTOR_SIZE;
    
    // If the requested bytes don't perfectly align with a sector, 
    // round up to ensure we cover the entire payload.
    if (total_bytes % ATA_SECTOR_SIZE != 0) {
        total_sectors++;
    }
    
    uint32_t current_lba = address;
    uint8_t* current_buffer_ptr = buffer;
    
    for (size_t i = 0; i < total_sectors; i++) {
        // Read a single 512-byte chunk into the current window of our large buffer
        if (!ata_read_sector(current_lba, current_buffer_ptr)) {
            return false; // Disk read failure or timeout
        }
        
        current_lba++;
        current_buffer_ptr += ATA_SECTOR_SIZE;
    }
    
    return true;
}

bool ata_write_bytes(uint32_t address, const uint8_t* buffer, size_t total_bytes) {
    if (buffer == NULL || total_bytes == 0) 
        return false;
    
    size_t total_sectors = total_bytes / ATA_SECTOR_SIZE;
    
    if (total_bytes % ATA_SECTOR_SIZE != 0) 
        total_sectors++;
    
    uint32_t current_lba = address;
    const uint8_t* current_buffer_ptr = buffer;
    
    for (size_t i = 0; i < total_sectors; i++) {
        if (!ata_write_sector(current_lba, current_buffer_ptr)) {
            return false;
        }
        
        current_lba++;
        current_buffer_ptr += ATA_SECTOR_SIZE;
    }
    
    return true;
}
