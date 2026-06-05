#ifndef KERNEL_DRIVERS_ATA_H
#define KERNEL_DRIVERS_ATA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Standard sector size for ATA/IDE drives
#define ATA_SECTOR_SIZE 512

// Initializes the drive context
bool ata_init(uint16_t io_base);

// Reads a specific sector from the disk into a 512-byte buffer
bool ata_read_sector(uint16_t io_base, uint32_t lba, uint8_t* buffer);

// Writes a 512-byte buffer out to a specific sector on the disk
bool ata_write_sector(uint16_t io_base, uint32_t lba, const uint8_t* buffer);

#endif
