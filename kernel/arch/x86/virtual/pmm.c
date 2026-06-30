#include <kernel/arch/x86/virtual/pmm.h>
#include <kernel/panic/panic_error.h>
#include <kernel/util/string.h>

#define MAX_PHYS_MEMORY   0xFFFFFFFF
#define PHYS_BITMAP_SIZE  (MAX_PHYS_MEMORY / PAGE_SIZE / 8U)

static uint32_t pmm_bitmap[PHYS_BITMAP_SIZE];
static uint32_t total_frames = 0;

void pmm_init(struct MultibootInfo* mbi, uint32_t physical_begin) {
    // Default to marking ALL physical memory as reserved/used (all bits 1)
    memset(pmm_bitmap, 0xFF, PHYS_BITMAP_SIZE * sizeof(uint32_t));
    
    if (!mbi || !(mbi->flags & (1 << 6))) {
        kernel_crashout(0x00, 0x00000000, 0x03, "Invalid memory map");
        return;
    }
    
    struct MultibootMmapEntry* mmap = (struct MultibootMmapEntry*)mbi->mmap_addr;
    uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
    
    // Free memory above the kernels core space
    while ((uint32_t)mmap < mmap_end) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t region_start = mmap->addr;
            uint64_t region_end = mmap->addr + mmap->len;
            
            if (region_end > MAX_PHYS_MEMORY) {
                region_end = MAX_PHYS_MEMORY;
            }
            
            // Align region_start UP to the next page, and region_end DOWN
            // This ensures we only mark fully-formed pages as free
            uint64_t aligned_start = (region_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t aligned_end = region_end & ~(PAGE_SIZE - 1);
            
            for (uint64_t addr = aligned_start; addr < aligned_end; addr += PAGE_SIZE) {
                if (addr < physical_begin) 
                    continue;
                
                uint32_t frame = addr / PAGE_SIZE;
                
                // Clear the allocation
                pmm_bitmap[frame / 32] &= ~(1U << (frame % 32));
                
                if (frame >= total_frames) {
                    total_frames = frame + 1;
                }
            }
        }
        mmap = (struct MultibootMmapEntry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < PHYS_BITMAP_SIZE; i++) {
        if (pmm_bitmap[i] == 0xFFFFFFFF) 
            continue;
        
        for (int bit = 0; bit < 32; bit++) {
            if ((pmm_bitmap[i] & (1U << bit)) == 0) {
                uint32_t frame = (i * 32) + bit;
                pmm_bitmap[i] |= (1U << bit); // Mark as used
                return frame * PAGE_SIZE;     // Returns physical address
            }
        }
    }
    
    // Out of physical frames
    return 0xFFFFFFFF;
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PAGE_SIZE;
    if (frame < total_frames) {
        pmm_bitmap[frame / 32] &= ~(1U << (frame % 32));
    }
}
