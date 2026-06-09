#include <kernel/arch/x86/virtual/pmm.h>
#include <kernel/panic/panic_error.h>
#include <kernel/util/string.h>

#define MAX_PHYS_MEMORY   (1024U * 1024U * 1024U * 4U) // 4GB
#define PHYS_BITMAP_SIZE  (MAX_PHYS_MEMORY / PAGE_SIZE / 8U)

static uint8_t pmm_bitmap[PHYS_BITMAP_SIZE];
static uint32_t total_frames = 0;

void pmm_init(struct MultibootInfo* mbi) {
    // Default to marking ALL physical memory as reserved/used
    memset(pmm_bitmap, 0xFF, PHYS_BITMAP_SIZE);
    
    // Ensure we actually received a valid memory map from the bootloader
    if (!mbi || !(mbi->flags & (1 << 6))) {
        kernel_crashout(0x00, 0x00000000, 0x03, "Invalid memory map");
        return;
    }
    
    // extern char _kernel_memory_end[];
    uint64_t _physical_memory_begin = 1024U * 1024U * 128U;
    
    struct MultibootMmapEntry* mmap = (struct MultibootMmapEntry*)mbi->mmap_addr;
    uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;
    
    // Loop through the memory map entries provided by GRUB
    while ((uint32_t)mmap < mmap_end) {
        
        // Check if this memory region is explicitly marked as usable RAM
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            
            uint64_t region_start = mmap->addr;
            uint64_t region_end = mmap->addr + mmap->len;
            
            // Cap the region to our max supported physical memory
            if (region_end > MAX_PHYS_MEMORY) {
                region_end = MAX_PHYS_MEMORY;
            }
            
            // Loop through the pages in this specific region
            for (uint64_t addr = region_start; addr < region_end; addr += PAGE_SIZE) {
                if (addr < _physical_memory_begin) 
                    continue;
                
                uint32_t frame = addr / PAGE_SIZE;
                
                // Clear the bit to mark this specific frame as FREE
                pmm_bitmap[frame / 8] &= ~(1U << (frame % 8));
                
                // Track total frames available to the PMM
                if (frame >= total_frames) {
                    total_frames = frame + 1;
                }
            }
        }
        
        // Advance to the next entry. The 'size' field tells you how many 
        // bytes ahead the next entry is, excluding the size field itself.
        mmap = (struct MultibootMmapEntry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
}

uint32_t pmm_alloc_frame(void) {
    uint32_t* bitmap32 = (uint32_t*)pmm_bitmap;
    uint32_t max_idx = PHYS_BITMAP_SIZE / 4;
    
    for (uint32_t i = 0; i < max_idx; i++) {
        // Check DWORD for available physical page
        if (bitmap32[i] != 0xFFFFFFFF) {
            // Now find the exact free bit in this 32-bit chunk
            for (int bit = 0; bit < 32; bit++) {
                if ((bitmap32[i] & (1U << bit)) == 0) {
                    uint32_t frame = (i * 32) + bit;
                    bitmap32[i] |= (1U << bit);
                    return frame * PAGE_SIZE;
                }
            }
        }
    }
    return 0xFFFFFFFF; // Real error code
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PAGE_SIZE;
    if (frame < total_frames) {
        pmm_bitmap[frame / 8] &= ~(1U << (frame % 8));
    }
}
