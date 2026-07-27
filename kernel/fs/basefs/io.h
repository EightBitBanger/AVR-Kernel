#ifndef BASE_IO_H
#define BASE_IO_H

#include <kernel/fs/fs.h>

#define FS_DEVICE_TYPE_ATA         0x0000
#define FS_DEVICE_TYPE_AHCI        0x0001
#define FS_DEVICE_TYPE_EEPROM      0x0002
#define FS_DEVICE_TYPE_RAW         0x0003

uint8_t fs_readb(uint32_t address);
void fs_writeb(uint32_t address, uint8_t byte);

void fs_mem_read(uint32_t address, void* destination, uint32_t size);
void fs_mem_write(uint32_t address, const void* source, uint32_t size);

#endif
