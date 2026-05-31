#include <stdint.h>
#include <stdbool.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/paging.h>

#define PAGE_SIZE 4096

// Main Page Directory
uint32_t page_directory[1024] __attribute__((aligned(4096)));

// 16 static tables to map the first 64 MB (Kernel, Stack, Backbuffer, Heap safety)
uint32_t static_page_tables[16][1024] __attribute__((aligned(4096)));

// Dynamic page table allocator tracking pointer.
// Any new page tables requested at runtime will be allocated starting right after the 64MB mark.
static uint32_t dynamic_table_watermark = 0x04000000; // 64 MB in hex

// High-level function to allocate a clean, blank page table dynamically
static uint32_t* allocate_dynamic_table(void) {
    uint32_t* table_ptr = (uint32_t*)dynamic_table_watermark;
    dynamic_table_watermark += PAGE_SIZE; // Move watermark to next 4KB page block

    // Zero out the new page table entries so it doesn't contain trash addresses
    for (int i = 0; i < 1024; i++) {
        table_ptr[i] = 0;
    }
    return table_ptr;
}

// MORPHED map_page: Maps any physical page to any virtual page dynamically!
void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    uint32_t* page_table = NULL;

    // Check if a page table already exists for this virtual address region
    if ((page_directory[pd_index] & VM_PRESENT) == 0) {
        // If it doesn't exist, allocate a new page table page on the fly!
        page_table = allocate_dynamic_table();
        page_directory[pd_index] = ((uint32_t)page_table) | VM_PRESENT | VM_READWRITE;
    } else {
        // If it does exist, extract the physical address of the page table (clear lower 12 bits)
        page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    }

    // Map the requested page frame into the table entry
    page_table[pt_index] = (physical_addr & ~0xFFF) | flags;
    
    // Invalidate Translation Lookaside Buffer (TLB) to force the CPU to use the new map
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void paging_initiate(struct MultibootInfo* mbi) {
    // 1. Clean out the main page directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // 2. FORCED INITIAL NET: Blanket map the first 64MB of physical memory.
    // This keeps your kernel execution completely safe.
    for (uint32_t t = 0; t < 16; t++) {
        page_directory[t] = ((uint32_t)&static_page_tables[t]) | VM_PRESENT | VM_READWRITE;

        for (uint32_t entry = 0; entry < 1024; entry++) {
            uint32_t physical_address = (t * 1024 * PAGE_SIZE) + (entry * PAGE_SIZE);
            static_page_tables[t][entry] = physical_address | VM_PRESENT | VM_READWRITE;
        }
    }

    // 3. DYNAMIC EXTENSION: Use our newly working map_page function 
    // to map the high-memory VRAM framebuffer, no matter how many pages it takes!
    uint32_t vram_start = mbi->framebuffer_addr;
    uint32_t vram_size  = mbi->framebuffer_pitch * mbi->framebuffer_height;
    uint32_t vram_end   = (vram_start + vram_size + 4095) & ~4095;

    // Loop through every single page frame of VRAM and map it dynamically
    for (uint32_t addr = vram_start; addr < vram_end; addr += PAGE_SIZE) {
        map_page(addr, addr, VM_PRESENT | VM_READWRITE);
    }

    // 4. COMMIT & FLIP PAGING ON
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Flip paging execution bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}
