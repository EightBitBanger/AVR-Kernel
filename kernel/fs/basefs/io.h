#ifndef BASE_IO_H
#define BASE_IO_H

#include <kernel/fs/basefs/structs.h>

#define FS_DEVICE_TYPE_ATA         0x0000
#define FS_DEVICE_TYPE_AHCI        0x0001
#define FS_DEVICE_TYPE_EEPROM      0x0002
#define FS_DEVICE_TYPE_RAW         0x0003

uint8_t fs_readb(struct FSDeviceContext* ctx, uint32_t address);
void fs_writeb(struct FSDeviceContext* ctx, uint32_t address, uint8_t byte);

void fs_mem_read(struct FSDeviceContext* ctx, uint32_t address, void* destination, uint32_t size);
void fs_mem_write(struct FSDeviceContext* ctx, uint32_t address, const void* source, uint32_t size);

void fs_cache_sync(struct FSDeviceContext* ctx);

#endif
