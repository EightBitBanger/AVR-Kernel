#include <kernel/arch/x86/page_alloc.h>
#include <kernel/arch/x86/paging.h>

#define PAGE_SIZE 4096

#define MAX_PHYS_MEMORY   (1024 * 1024 * 1024) // Manage up to 1 GB of physical RAM
#define RESERVED_MEMORY   (128 * 1024 * 1024)  // Reserve first 128 MB (Kernel + Page Tables)

#define VM_START          0x10000000           // Start allocating virtual space at 256 MB Mark
#define VM_END            0x40000000           // End at 1 GB Mark (768 MB total pool space)
#define VM_SIZE           (VM_END - VM_START)

#define PHYS_BITMAP_SIZE  (MAX_PHYS_MEMORY / PAGE_SIZE / 8)
#define VIRT_BITMAP_SIZE  (VM_SIZE / PAGE_SIZE / 8)

static uint8_t phys_bitmap[PHYS_BITMAP_SIZE];
static uint8_t virt_bitmap[VIRT_BITMAP_SIZE];

// Access the global page directory defined in paging.c
extern uint32_t page_directory[1024];

// Scans a bitmap to find a contiguous block of zero (free) bits
static int find_contiguous_bits(uint8_t* bitmap, size_t bitmap_size, size_t num_bits) {
    size_t count = 0;
    int start_bit = -1;
    size_t total_bits = bitmap_size * 8;
    
    for (size_t i = 0; i < total_bits; i++) {
        bool is_allocated = (bitmap[i / 8] & (1 << (i % 8))) != 0;
        
        if (!is_allocated) {
            if (count == 0) {
                start_bit = (int)i;
            }
            count++;
            if (count == num_bits) {
                return start_bit;
            }
        } else {
            count = 0;
            start_bit = -1;
        }
    }
    return -1;
}

// Finds a single free physical frame index
static int find_free_physical_frame(void) {
    size_t total_bits = PHYS_BITMAP_SIZE * 8;
    for (size_t i = 0; i < total_bits; i++) {
        if ((phys_bitmap[i / 8] & (1 << (i % 8))) == 0) {
            return (int)i;
        }
    }
    return -1;
}


void page_allocator_init(struct MultibootInfo* mbi) {
    // Mark all physical pages as reserved/allocated initially
    for (size_t i = 0; i < PHYS_BITMAP_SIZE; i++) {
        phys_bitmap[i] = 0xFF;
    }
    // Mark all managed virtual pages as free
    for (size_t i = 0; i < VIRT_BITMAP_SIZE; i++) {
        virt_bitmap[i] = 0x00;
    }
    
    // Detect total physical memory from multiboot structure
    uint32_t total_mem_bytes = MAX_PHYS_MEMORY;
    if (mbi && mbi->mem_upper > 0) {
        // mbi->mem_upper indicates upper memory in KB starting from the 1MB mark
        uint32_t total_mem_kb = 1024 + mbi->mem_upper;
        total_mem_bytes = total_mem_kb * 1024;
    }
    
    if (total_mem_bytes > MAX_PHYS_MEMORY) {
        total_mem_bytes = MAX_PHYS_MEMORY;
    }
    
    // Free physical frames above the 128 MB reservation threshold up to available RAM
    uint32_t start_frame = RESERVED_MEMORY / PAGE_SIZE;
    uint32_t end_frame = total_mem_bytes / PAGE_SIZE;
    
    for (uint32_t frame = start_frame; frame < end_frame; frame++) {
        phys_bitmap[frame / 8] &= ~(1 << (frame % 8));
    }
}

void* alloc_pages(size_t num_pages) {
    if (num_pages == 0) return NULL;
    
    // Search for a large enough continuous segment in the virtual space
    int virt_start_page = find_contiguous_bits(virt_bitmap, VIRT_BITMAP_SIZE, num_pages);
    if (virt_start_page == -1) {
        return NULL; // Out of Virtual Memory space
    }
    
    uint32_t start_vaddr = VM_START + (virt_start_page * PAGE_SIZE);
    size_t pages_mapped = 0;
    
    // Map each virtual page to an available physical frame
    for (size_t i = 0; i < num_pages; i++) {
        int phys_frame = find_free_physical_frame();
        if (phys_frame == -1) {
            // Out of physical RAM! Roll back everything allocated in this cycle
            for (size_t j = 0; j < pages_mapped; j++) {
                uint32_t rollback_vaddr = start_vaddr + (j * PAGE_SIZE);
                uint32_t pd_idx = rollback_vaddr >> 22;
                uint32_t pt_idx = (rollback_vaddr >> 12) & 0x3FF;
                
                uint32_t* page_table = (uint32_t*)(page_directory[pd_idx] & ~0xFFF);
                uint32_t phys_addr = page_table[pt_idx] & ~0xFFF;
                size_t p_frame = phys_addr / PAGE_SIZE;
                
                // Free the physical frame back to the bitmap pool
                phys_bitmap[p_frame / 8] &= ~(1 << (p_frame % 8));
                page_table[pt_idx] = 0; // Unmap
                __asm__ volatile("invlpg (%0)" : : "r"(rollback_vaddr) : "memory");
            }
            return NULL;
        }
        
        // Mark physical frame as allocated
        phys_bitmap[phys_frame / 8] |= (1 << (phys_frame % 8));
        
        uint32_t phys_addr = phys_frame * PAGE_SIZE;
        uint32_t current_vaddr = start_vaddr + (i * PAGE_SIZE);
        
        // Bind the physical frame to the virtual address using your map_page implementation
        map_page(phys_addr, current_vaddr, VM_PRESENT | VM_READWRITE);
        pages_mapped++;
    }
    
    // Finalize allocation by marking virtual pages as occupied
    for (size_t i = 0; i < num_pages; i++) {
        size_t bit = virt_start_page + i;
        virt_bitmap[bit / 8] |= (1 << (bit % 8));
    }
    
    return (void*)start_vaddr;
}

void* alloc_virt_mmio(uint32_t phys_addr, size_t num_pages) {
    if (num_pages == 0) return NULL;
    
    // Search for a large enough continuous segment in the managed virtual space pool
    int virt_start_page = find_contiguous_bits(virt_bitmap, VIRT_BITMAP_SIZE, num_pages);
    if (virt_start_page == -1) {
        return NULL; // Out of Virtual Memory space
    }
    
    // Calculate the unique virtual address block starting point
    uint32_t start_vaddr = VM_START + (virt_start_page * PAGE_SIZE);
    
    // Map each unique virtual page to the fixed hardware physical address
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t current_vaddr = start_vaddr + (i * PAGE_SIZE);
        uint32_t current_paddr = phys_addr + (i * PAGE_SIZE);
        
        map_page(current_paddr, current_vaddr, VM_PRESENT | VM_READWRITE);
    }
    
    // Finalize allocation by marking these virtual pages as occupied
    // This ensures regular 'alloc_pages' won't hand this virtual address out to anyone else
    for (size_t i = 0; i < num_pages; i++) {
        size_t bit = virt_start_page + i;
        virt_bitmap[bit / 8] |= (1 << (bit % 8));
    }
    
    return (void*)start_vaddr;
}

void free_pages(void* virtual_addr, size_t num_pages) {
    if (!virtual_addr || num_pages == 0) return;
    
    uint32_t start_vaddr = (uint32_t)virtual_addr;
    
    // Bounds and structural verification check
    if (start_vaddr < VM_START || start_vaddr >= VM_END || (start_vaddr % PAGE_SIZE) != 0) 
        return; 
    
    size_t virt_start_page = (start_vaddr - VM_START) / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t vaddr = start_vaddr + (i * PAGE_SIZE);
        size_t bit = virt_start_page + i;
        
        uint32_t pd_index = vaddr >> 22;
        uint32_t pt_index = (vaddr >> 12) & 0x3FF;
        
        // Trace the entry through page table directories back to the physical frame
        if (page_directory[pd_index] & VM_PRESENT) {
            uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
            if (page_table[pt_index] & VM_PRESENT) {
                uint32_t phys_addr = page_table[pt_index] & ~0xFFF;
                size_t phys_frame = phys_addr / PAGE_SIZE;
                
                // Mark physical frame as free
                if (phys_frame < (MAX_PHYS_MEMORY / PAGE_SIZE)) {
                    phys_bitmap[phys_frame / 8] &= ~(1 << (phys_frame % 8));
                }
                
                // Unmap the virtual page entry and flush TLB
                page_table[pt_index] = 0;
                __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
            }
        }
        
        // Mark virtual page as free in the virtual bitmap
        if (bit < (VM_SIZE / PAGE_SIZE)) {
            virt_bitmap[bit / 8] &= ~(1 << (bit % 8));
        }
    }
}
