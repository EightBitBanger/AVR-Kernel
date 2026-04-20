#ifndef KERNEL_MEMORY_HEAP_H
#define KERNEL_MEMORY_HEAP_H

#include <stdint.h>

#define KMALLOC_NULL         0xFFFFFFFF

#define KMALLOC_FLAG_DIRTY       0x01
#define KMALLOC_FLAG_MOUNT       0x02
#define KMALLOC_FLAG_DIRECTORY   0x04
#define KMALLOC_FLAG_EXTENT      0x08

#define KMALLOC_PERMISSION_EXECUTABLE  0x01
#define KMALLOC_PERMISSION_READ        0x02
#define KMALLOC_PERMISSION_WRITE       0x04
#define KMALLOC_PERMISSION_USER        0x08

#define KMALLOC_TYPE_DEVICE            0x01
#define KMALLOC_TYPE_DRIVER            0x02
#define KMALLOC_TYPE_BLOCK             0x04
#define KMALLOC_TYPE_RAW               0x08
#define KMALLOC_TYPE_PROCBLOCK         0x10

struct __attribute__((packed)) KMallocHeader {
    uint32_t size;
    uint8_t  flags;
    uint8_t  type;
    uint8_t  perm;
    uint8_t  magic;
};

void heap_init(uint32_t block_size, uint32_t total_memory);

uint32_t kmalloc(uint32_t size);
void kfree(uint32_t address);

uint32_t kmalloc_next(uint32_t previous_address);

void kmem_write(uint32_t address, const void* source, uint32_t size);
void kmem_read(uint32_t address, void* destination, uint32_t size);

uint8_t kmalloc_get_flags(uint32_t address);
void    kmalloc_set_flags(uint32_t address, uint8_t flags);

uint8_t kmalloc_get_type(uint32_t address);
void    kmalloc_set_type(uint32_t address, uint8_t type);

uint8_t kmalloc_get_permissions(uint32_t address);
void    kmalloc_set_permissions(uint32_t address, uint8_t type);

uint32_t kmalloc_get_size(uint32_t address);

void kmalloc_bitmap_set(uint8_t* bitmap, uint8_t* bitmap_dirty);
void kmalloc_bitmap_write(void);
void kmalloc_bitmap_read(void);

uint32_t heap_get_block_size(void);
uint32_t heap_get_total_memory(void);

void heap_set_base_address(uint32_t base);
uint32_t heap_get_base_address(void);

#endif
