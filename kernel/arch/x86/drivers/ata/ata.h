#ifndef _KERNEL_DRIVERS_ATA_H_
#define _KERNEL_DRIVERS_ATA_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ATA_SECTOR_SIZE 512

bool ata_init(uint16_t io_base);

bool ata_read_sector(uint32_t address, uint8_t* buffer);
bool ata_write_sector(uint32_t address, const uint8_t* buffer);

bool ata_read_bytes(uint32_t address, uint8_t* buffer, size_t total_bytes);
bool ata_write_bytes(uint32_t address, const uint8_t* buffer, size_t total_bytes);

#endif
