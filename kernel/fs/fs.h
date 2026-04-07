#ifndef BASE_FILE_SYSTEM_H
#define BASE_FILE_SYSTEM_H

#include <kernel/device/device.h>

#include <stdint.h>
#include <stdlib.h>

#define FS_MAGIC        0xAAUL

#define FS_NULL         0xFFFFFFFFUL

struct FSHeaderBlock {
    uint32_t size;
    uint8_t  flags;
    uint8_t  type;
    uint8_t  reserved;
    uint8_t  magic;
};

#define FS_HEADER_SIZE  ((uint32_t)sizeof(struct FSHeaderBlock))

void fs_init(void);

uint32_t fs_device_open(uint32_t device_address, uint8_t* bitmap, uint32_t sector_size, uint32_t total_size);

uint32_t fs_create_file(void* buffer, uint32_t size);
void fs_destroy_file(uint32_t address);


//uint32_t fs_alloc(uint32_t size);
//void fs_free(uint32_t address);


void fs_bitmap_write(void);
void fs_bitmap_read(void);

//void fs_writeb(uint32_t address, uint8_t byte);
//void fs_readb(uint32_t address, uint8_t* byte);

#endif
