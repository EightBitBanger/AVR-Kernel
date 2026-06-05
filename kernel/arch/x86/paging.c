#include <stdint.h>
#include <stdbool.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/paging.h>

#define PAGE_SIZE 4096

// Main Page Directory
uint32_t page_directory[1024] __attribute__((aligned(4096)));

// 32 static tables to map the first 128 MB
uint32_t static_page_tables[32][1024] __attribute__((aligned(4096)));

// Dynamic page table allocator tracking pointer.
// Any new page tables requested at runtime will be allocated starting right after the 64MB mark.
static uint32_t dynamic_table_watermark = 0x04000000;

static uint32_t* allocate_dynamic_table(void) {
    uint32_t* table_ptr = (uint32_t*)dynamic_table_watermark;
    dynamic_table_watermark += PAGE_SIZE; // Move watermark to next 4KB page block
    
    // Zero out the new page table entries
    for (int i = 0; i < 1024; i++) {
        table_ptr[i] = 0;
    }
    return table_ptr;
}

void map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    
    uint32_t* page_table = NULL;
    
    // Check if a page table already exists for this virtual address region
    if ((page_directory[pd_index] & VM_PRESENT) == 0) {
        // Allocate a new page table page
        page_table = allocate_dynamic_table();
        page_directory[pd_index] = ((uint32_t)page_table) | VM_PRESENT | VM_READWRITE;
    } else {
        // Extract the physical address of the page table (clear lower 12 bits)
        page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    }
    
    // Map the requested page frame into the table entry
    page_table[pt_index] = (physical_addr & ~0xFFF) | flags;
    
    // Invalidate Translation Lookaside Buffer (TLB) to force the CPU to use the new map
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void paging_initiate(struct MultibootInfo* mbi) {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }
    
    // Map the first 128MB of physical memory as kernel memory
    // Map the first 128MB of physical memory
    for (uint32_t t = 0; t < 32; t++) { // Change 16 to 32
        page_directory[t] = ((uint32_t)&static_page_tables[t]) | VM_PRESENT | VM_READWRITE;
        
        for (uint32_t entry = 0; entry < 1024; entry++) {
            uint32_t physical_address = (t * 1024 * PAGE_SIZE) + (entry * PAGE_SIZE);
            static_page_tables[t][entry] = physical_address | VM_PRESENT | VM_READWRITE;
        }
    }
    
    // Map VRAM memory block into virtual memory
    uint32_t vram_start = mbi->framebuffer_addr;
    uint32_t vram_size  = mbi->framebuffer_pitch * mbi->framebuffer_height;
    uint32_t vram_end   = (vram_start + vram_size + 4095) & ~4095;
    
    // Assign all VRAM pages and map it dynamically
    for (uint32_t addr = vram_start; addr < vram_end; addr += PAGE_SIZE) {
        map_page(addr, addr, VM_PRESENT | VM_READWRITE);
    }
    
    // Commit & flip paging on
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));
    
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Flip paging execution bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}
