#ifndef KERNEL_MEMORY_PAGING_H
#define KERNEL_MEMORY_PAGING_H

#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/drivers/multiboot_info.h>

#define VM_PRESENT    0x1
#define VM_READWRITE  0x2

void paging_initiate(struct MultibootInfo* mbi);

void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags);

#endif
