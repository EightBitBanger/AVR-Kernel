#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/util/string.h>
#include <stdbool.h>

#define VM_START          0x10000000U           
#define VM_END            0x40000000U           
#define VM_SIZE           (VM_END - VM_START)
#define VIRT_BITMAP_SIZE  (VM_SIZE / PAGE_SIZE / 8)

#define VM_PAGE_DIR_SIZE     1024
#define VM_PAGE_TABLE_SIZE   1024

static uint8_t virt_bitmap[VIRT_BITMAP_SIZE];

static int find_contiguous_bits(uint8_t* bitmap, size_t bitmap_size, size_t num_bits);

// Statically reserve a full page directory and all page tables in BSS using defines
uint32_t page_directory[VM_PAGE_DIR_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint32_t static_page_tables[VM_PAGE_DIR_SIZE][VM_PAGE_TABLE_SIZE] __attribute__((aligned(PAGE_SIZE)));

void vmm_init(struct MultibootInfo* mbi, uint32_t identity_map_size) {
    memset(virt_bitmap, 0x00, VIRT_BITMAP_SIZE);
    
    // Link every directory entry to its corresponding static page table
    for (uint32_t pd_idx = 0; pd_idx < VM_PAGE_DIR_SIZE; pd_idx++) {
        page_directory[pd_idx] = ((uint32_t)&static_page_tables[pd_idx]) | VM_PRESENT | VM_READWRITE;
        
        // Identity map only up to the specified identity_map_size
        for (uint32_t pt_idx = 0; pt_idx < VM_PAGE_TABLE_SIZE; pt_idx++) {
            uint32_t physical_address = (pd_idx * VM_PAGE_TABLE_SIZE * PAGE_SIZE) + (pt_idx * PAGE_SIZE);
            
            if (physical_address < identity_map_size) {
                static_page_tables[pd_idx][pt_idx] = physical_address | VM_PRESENT | VM_READWRITE;
            } else {
                // Explicitly clear entries outside the requested range to ensure they are marked "not present"
                static_page_tables[pd_idx][pt_idx] = 0;
            }
        }
    }
    
    //
    // Initiate MMU
    //
    
    // Hand the root directory pointer over to the CPU's CR3 control register
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));
    
    // Instruct the hardware MMU to flip on address translation execution
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U; 
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void* vmm_alloc_pages(size_t num_pages) {
    int virt_start_page = find_contiguous_bits(virt_bitmap, VIRT_BITMAP_SIZE, num_pages);
    if (virt_start_page == -1) return NULL;
    
    uint32_t start_vaddr = VM_START + (virt_start_page * PAGE_SIZE);
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t current_vaddr = start_vaddr + (i * PAGE_SIZE);
        uint32_t phys_frame = pmm_alloc_frame();
        
        if (phys_frame == 0) {
            vmm_free_pages((void*)start_vaddr, i);
            return NULL;
        }
        
        vmm_map_page(phys_frame, current_vaddr, VM_PRESENT | VM_READWRITE);
        
        size_t bit = (size_t)virt_start_page + i;
        virt_bitmap[bit / 8] |= (1U << (bit % 8));
    }
    
    return (void*)start_vaddr;
}

void vmm_free_pages(void* virtual_addr, size_t num_pages) {
    uint32_t vaddr = (uint32_t)virtual_addr;
    size_t virt_start_page = (vaddr - VM_START) / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t current_vaddr = vaddr + (i * PAGE_SIZE);
        
        // Re-identity map back to its default clean address space profile
        vmm_map_page(current_vaddr, current_vaddr, VM_PRESENT | VM_READWRITE);
        
        size_t bit = virt_start_page + i;
        virt_bitmap[bit / 8] &= ~(1U << (bit % 8));
    }
}

void vmm_map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FFU;
    
    // Every single page table is already allocated and present! 
    // We simply update the table entry directly via its identity-mapped pointer.
    static_page_tables[pd_index][pt_index] = (physical_addr & ~0xFFFU) | flags;
    
    // Clear out stale CPU caches for this specific address mapping alteration
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void vmm_map_hardware_region(uint32_t phys_addr, uint32_t virt_addr, uint32_t size, uint32_t flags) {
    // Align the starting addresses down to the nearest page boundary
    uint32_t page_offset = phys_addr & (PAGE_SIZE - 1);
    uint32_t start_phys  = phys_addr & ~(PAGE_SIZE - 1);
    uint32_t start_virt  = virt_addr & ~(PAGE_SIZE - 1);
    
    // Adjust total size to account for the starting offset misalignment
    uint32_t total_size  = size + page_offset;
    
    // Round total size up to the nearest multiple of PAGE_SIZE
    uint32_t num_pages   = (total_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    
    // Iterate through every page, mapping physical frames to distinct virtual destinations
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t current_phys = start_phys + (i * PAGE_SIZE);
        uint32_t current_virt = start_virt + (i * PAGE_SIZE);
        
        vmm_map_page(current_phys, current_virt, flags);
    }
}

static int find_contiguous_bits(uint8_t* bitmap, size_t bitmap_size, size_t num_bits) {
    size_t count = 0;
    int start_bit = -1;
    size_t total_bits = bitmap_size * 8;
    
    for (size_t i = 0; i < total_bits; i++) {
        bool is_allocated = (bitmap[i / 8] & (1U << (i % 8))) != 0;
        if (!is_allocated) {
            if (count == 0) start_bit = (int)i;
            count++;
            if (count == num_bits) return start_bit;
        } else {
            count = 0;
            start_bit = -1;
        }
    }
    return -1;
}
