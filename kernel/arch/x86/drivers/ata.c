#include <kernel/arch/x86/drivers/ata.h>
#include <kernel/arch/x86/io.h> // Assumes your inb, outb, inw, outw live here

// Helper to poll the status register until the drive is ready
static inline bool ata_wait_ready(uint16_t io_base, uint8_t mask, uint8_t value, uint32_t timeout) {
    for (uint32_t i = 0; i < timeout; i++) {
        uint8_t status = inb(io_base + 7);
        // Ensure BUSY (0x80) is clear, and our target condition matches
        if (!(status & 0x80) && ((status & mask) == value)) {
            return true;
        }
    }
    return false;
}

bool ata_init(uint16_t io_base) {
    // Soft reset / Ping the device to ensure it's responsive
    outb(io_base + 6, 0xE0); // Select Master Drive, LBA mode
    
    // An optional short delay to let the controller settle
    for(volatile int i = 0; i < 1000; i++); 
    
    uint8_t status = inb(io_base + 7);
    if (status == 0xFF) {
        // 0xFF means nothing is connected to this I/O base port
        return false; 
    }
    return true;
}

bool ata_read_sector(uint16_t io_base, uint32_t lba, uint8_t* buffer) {
    uint16_t* word_buf = (uint16_t*)buffer;

    // 1. Send Drive and Bits 24-27 of LBA
    outb(io_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 2. Sector Count (1 sector)
    outb(io_base + 2, 1);
    
    // 3. Send remaining LBA bytes
    outb(io_base + 3, (uint8_t)lba);         // LBA Low (0-7)
    outb(io_base + 4, (uint8_t)(lba >> 8));  // LBA Mid (8-15)
    outb(io_base + 5, (uint8_t)(lba >> 16)); // LBA High (16-23)
    
    // 4. Send Read Command (0x20)
    outb(io_base + 7, 0x20);
    
    // 5. Wait until Data Request bit (0x08) is set
    if (!ata_wait_ready(io_base, 0x08, 0x08, 100000)) {
        return false; // Timeout or disk error
    }
    
    // 6. Read 256 words (512 bytes total) from the Data Port
    for (int i = 0; i < 256; i++) {
        word_buf[i] = inw(io_base + 0);
    }
    
    return true;
}

bool ata_write_sector(uint16_t io_base, uint32_t lba, const uint8_t* buffer) {
    const uint16_t* word_buf = (const uint16_t*)buffer;

    // 1. Send Drive and Bits 24-27 of LBA
    outb(io_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 2. Sector Count (1 sector)
    outb(io_base + 2, 1);
    
    // 3. Send remaining LBA bytes
    outb(io_base + 3, (uint8_t)lba);
    outb(io_base + 4, (uint8_t)(lba >> 8));
    outb(io_base + 5, (uint8_t)(lba >> 16));
    
    // 4. Send Write Command (0x30)
    outb(io_base + 7, 0x30);
    
    // 5. Wait until drive clears BUSY and sets Data Request (0x08), ready to consume data
    if (!ata_wait_ready(io_base, 0x08, 0x08, 100000)) {
        return false;
    }
    
    // 6. Stream the 256 words (512 bytes) *out* to the Data Port
    for (int i = 0; i < 256; i++) {
        outw(io_base + 0, word_buf[i]);
    }
    
    // 7. Send an ATA Flush Cache command (0xE7) to ensure physical commit to storage
    outb(io_base + 7, 0xE7);
    ata_wait_ready(io_base, 0x00, 0x00, 100000); // Wait for processing to stop
    
    return true;
}
