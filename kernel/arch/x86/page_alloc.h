#ifndef KERNEL_MEMORY_PAGE_ALLOC_H
#define KERNEL_MEMORY_PAGE_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>

// Initialize the physical and virtual bitmaps based on Multiboot memory info
void page_allocator_init(struct MultibootInfo* mbi);

// Allocate a contiguous sequence of virtual pages, map them to physical frames, and return the virtual address
void* alloc_pages(size_t num_pages);

// Allocate a unique sequence of virtual pages and map them to a specific physical MMIO address
void* alloc_virt_mmio(uint32_t phys_addr, size_t num_pages);

// Unmap and free a previously allocated contiguous block of virtual pages
void free_pages(void* virtual_addr, size_t num_pages);

#endif
