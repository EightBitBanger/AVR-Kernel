#ifndef KERNEL_MEMORY_HEAP_H
#define KERNEL_MEMORY_HEAP_H

#include <stdint.h>

#define KMALLOC_NULL         0xFFFFFFFFUL

#define KMALLOC_FLAG_DEVICE      0x01
#define KMALLOC_FLAG_DRIVER      0x02
#define KMALLOC_FLAG_BLOCK       0x04
#define KMALLOC_FLAG_RAW         0x08
#define KMALLOC_FLAG_FS          0x10

#define KMALLOC_FLAG_READ        0x20
#define KMALLOC_FLAG_WRITE       0x40
#define KMALLOC_FLAG_SYSTEM      0x80

#define KMALLOC_TYPE_EXECUTABLE  0x01
#define KMALLOC_TYPE_DIRECTORY   0x02
#define KMALLOC_TYPE_FILE        0x04

void heap_init(uint32_t block_size, uint32_t total_memory);

void heap_set_base_address(uint32_t base);
uint32_t heap_get_base_address(void);

struct KmallocHeader {
    uint32_t size;
    uint8_t  flags;
    uint8_t  type;
    uint8_t  reserved;
    uint8_t  magic;
};

uint32_t kmalloc(uint32_t size);
void kfree(uint32_t address);

uint32_t kmalloc_next(uint32_t previous_address);

void kmem_write(uint32_t address, const void* source, uint32_t size);
void kmem_read(uint32_t address, void* destination, uint32_t size);

uint8_t  kmalloc_get_flags(uint32_t address);
void     kmalloc_set_flags(uint32_t address, uint8_t flags);

uint8_t  kmalloc_get_type(uint32_t address);
void     kmalloc_set_type(uint32_t address, uint8_t type);

uint32_t kmalloc_get_size(uint32_t address);

void kmalloc_bitmap_set(uint8_t* bitmap);
void kmalloc_bitmap_write(void);
void kmalloc_bitmap_read(void);

#endif
