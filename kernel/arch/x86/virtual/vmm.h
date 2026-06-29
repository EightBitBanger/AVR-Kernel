#ifndef _KERNEL_VIRTUAL_MEMORY_MGR_H_
#define _KERNEL_VIRTUAL_MEMORY_MGR_H_

#include <stddef.h>
#include <stdint.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>
#include <kernel/arch/x86/virtual/pmm.h>

// Core Control Flags
#define VM_PRESENT       0x001U  // Bit 0: Page is in memory
#define VM_READWRITE     0x002U  // Bit 1: 0 = Read-Only, 1 = Read-Write
#define VM_USER          0x004U  // Bit 2: 0 = Supervisor, 1 = User space

// Cache Configuration Flags
#define VM_PWT           0x008U  // Bit 3: Write-Through caching
#define VM_PCD           0x010U  // Bit 4: Cache Disable

// CPU Profiling Flags
#define VM_ACCESSED      0x020U  // Bit 5: Read or written by CPU
#define VM_DIRTY         0x040U  // Bit 6: Written by CPU

// Advanced Attributes
#define VM_PAT_PTE       0x080U  // Bit 7: Page Attribute Table index (for 4KB pages)
#define VM_GLOBAL        0x100U  // Bit 8: Global page (requires CR4.PGE)
#define VM_PAT_PDE       0x800U  // Bit 12: Page Attribute Table index (for 4MB pages)

#define VM_WRITE_COMBINING (VM_PRESENT | VM_READWRITE | VM_PWT | VM_PCD)

extern uint32_t page_directory[];

void vmm_init(struct MultibootInfo* mbi, uint32_t identity_map_size);

void* vmm_alloc_pages(size_t num_pages);
void vmm_free_pages(void* virtual_addr, size_t num_pages);
void vmm_unmap_page(uint32_t virtual_addr);

// Map a physical address into a virtual address space
void vmm_map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags);

// Map a physical hardware address into a virtual address space
void vmm_map_hardware_region(uint32_t phys_addr, uint32_t virt_addr, uint32_t size, uint32_t flags);

// Map an MMIO region to a virtual memory address
void* vmm_map_mmio_region(uint32_t phys_addr, uint32_t size_bytes, uint32_t flags);

// Translate a virtual address into a physical address
uint32_t vmm_get_phys_addr(void* virtual_addr);

// Translate a physical address into a virtual address
void* vmm_get_virt_addr(uint32_t physical_addr);

#endif
