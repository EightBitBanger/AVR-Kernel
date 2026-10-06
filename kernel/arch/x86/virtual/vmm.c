#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/panic/panic_error.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/irq.h>
#include <stdbool.h>

#define VM_START          0x10000000U           
#define VM_END            0x20000000U           
#define VM_SIZE           (VM_END - VM_START)
#define VIRT_BITMAP_SIZE  (VM_SIZE / PAGE_SIZE / 8)

#define VM_PAGE_DIR_SIZE     1024
#define VM_PAGE_TABLE_SIZE   1024

static uint8_t virt_bitmap[VIRT_BITMAP_SIZE];

static int find_contiguous_bits(uint8_t* bitmap, size_t bitmap_size, size_t num_bits);

// Statically reserve a full page directory and all page tables in BSS
uint32_t page_directory[VM_PAGE_DIR_SIZE] __attribute__((aligned(PAGE_SIZE)));
static uint32_t static_page_tables[VM_PAGE_DIR_SIZE][VM_PAGE_TABLE_SIZE] __attribute__((aligned(PAGE_SIZE)));

//
// Page Attribute Table (write-combining support)
//

#define MSR_IA32_PAT        0x277U
#define PAT_TYPE_WC         0x01U
#define CPUID_EDX_PAT       (1U << 16)
#define CR0_CD              (1U << 30)
#define CR0_NW              (1U << 29)

// Fallback when PAT is missing: uncached, the previous behavior
static uint32_t vm_wc_flags = VM_PRESENT | VM_READWRITE | VM_PWT | VM_PCD;
static bool     vm_wc_available = false;

uint32_t vmm_get_write_combining_flags(void) {
    return vm_wc_flags;
}

bool vmm_has_write_combining(void) {
    return vm_wc_available;
}

// Reprogram PAT entry 1 (selected by PWT=1, PCD=0, PAT=0) from its power-on
// default WT to WC. Entries 0, 2 and 3 keep their defaults (WB, UC-, UC), so
// existing mappings (plain = WB, MMIO with PCD = UC-) are unaffected. Nothing
// maps with PWT alone before this runs.
//
// Follows the SDM sequence: caches off, flush, write MSR, flush TLB, caches on.
static void vmm_pat_init(void) {
    uint32_t eax = 1, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (!(edx & CPUID_EDX_PAT))
        return;

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(MSR_IA32_PAT));
    lo = (lo & ~0x0000FF00U) | (PAT_TYPE_WC << 8);

    uint32_t irq_flags = irq_save();

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0" : : "r"((cr0 | CR0_CD) & ~CR0_NW) : "memory");
    __asm__ volatile("wbinvd" ::: "memory");

    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(MSR_IA32_PAT) : "memory");

    // Flush the whole (non-global) TLB by reloading CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    __asm__ volatile("wbinvd" ::: "memory");
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    irq_restore(irq_flags);

    vm_wc_flags     = VM_PRESENT | VM_READWRITE | VM_PWT;
    vm_wc_available = true;
}

void vmm_init(struct MultibootInfo* mbi, uint32_t identity_map_size) {
    memset(virt_bitmap, 0x00, VIRT_BITMAP_SIZE);
    
    // Completely clear the page directory. Everything begins as "not present"
    memset(page_directory, 0, sizeof(page_directory));
    
    // Clear out all static page tables to prevent garbage entries
    memset(static_page_tables, 0, sizeof(static_page_tables));
    
    // Handle Identity Mapping up to identity_map_size
    for (uint32_t pd_idx = 0; pd_idx < VM_PAGE_DIR_SIZE; pd_idx++) {
        bool dir_has_mappings = false;
        
        for (uint32_t pt_idx = 0; pt_idx < VM_PAGE_TABLE_SIZE; pt_idx++) {
            uint32_t physical_address = (pd_idx * VM_PAGE_TABLE_SIZE * PAGE_SIZE) + (pt_idx * PAGE_SIZE);
            
            if (physical_address < identity_map_size) {
                static_page_tables[pd_idx][pt_idx] = physical_address | VM_PRESENT | VM_READWRITE;
                dir_has_mappings = true;
            }
        }
        
        // Only link the directory entry and mark it present if this page table holds an identity mapping
        if (dir_has_mappings) {
            page_directory[pd_idx] = ((uint32_t)&static_page_tables[pd_idx]) | VM_PRESENT | VM_READWRITE;
        }
    }
    
    //
    // Initiate MMU
    //
    
    // Hand the root directory pointer over to the CPU's CR3 control register
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));
    
    // Switch on the hardware MMU to activate address translation execution
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    // Enable write-combining memory type before anything maps the framebuffer
    vmm_pat_init();
}

void* vmm_alloc_pages(size_t num_pages) {
    uint32_t irq_flags = irq_save();
    
    int virt_start_page = find_contiguous_bits(virt_bitmap, VIRT_BITMAP_SIZE, num_pages);
    if (virt_start_page == -1) {
        irq_restore(irq_flags);
        kernel_crashout(0x00, 0x00000000, 0x03, "Out of physical memory");
        return NULL;
    }
    
    uint32_t start_vaddr = VM_START + (virt_start_page * PAGE_SIZE);
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t current_vaddr = start_vaddr + (i * PAGE_SIZE);
        uint32_t phys_frame = pmm_alloc_frame();
        
        if (phys_frame == PMM_NO_FRAME) {
            vmm_free_pages((void*)start_vaddr, i);
            irq_restore(irq_flags);
            
            kernel_crashout(0x00, 0x00000000, 0x03, "Out of physical memory");
            return NULL;
        }
        
        vmm_map_page(phys_frame, current_vaddr, VM_PRESENT | VM_READWRITE);
        
        size_t bit = (size_t)virt_start_page + i;
        virt_bitmap[bit / 8] |= (1U << (bit % 8));
    }
    
    irq_restore(irq_flags);
    return (void*)start_vaddr;
}

void vmm_free_pages(void* virtual_addr, size_t num_pages) {
    // Round DOWN to the containing page (rounding up would skip a page)
    uint32_t vaddr = (uint32_t)virtual_addr & ~0xFFFU;
    if (vaddr < VM_START || vaddr >= VM_END) return;
    
    uint32_t irq_flags = irq_save();
    size_t virt_start_page = (vaddr - VM_START) / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t current_vaddr = vaddr + (i * PAGE_SIZE);
        
        // Get the physical frame so we can return it to the PMM
        uint32_t pd_index = current_vaddr >> 22;
        uint32_t pt_index = (current_vaddr >> 12) & 0x3FFU;
        uint32_t pte = static_page_tables[pd_index][pt_index];
        
        if (pte & VM_PRESENT) {
            uint32_t phys_frame = pte & ~0xFFFU;
            pmm_free_frame(phys_frame);
        }
        
        // Clear the mapping from the page tables
        vmm_unmap_page(current_vaddr);
        
        // Clear our virtual allocation bitmap track tracker
        size_t bit = virt_start_page + i;
        virt_bitmap[bit / 8] &= ~(1U << (bit % 8));
    }
    
    irq_restore(irq_flags);
}

void vmm_unmap_page(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FFU;
    
    // Only clear if the directory table says a page table actually exists
    if (page_directory[pd_index] & VM_PRESENT) {
        // Break the mapping by zeroing the entry out (clears VM_PRESENT)
        static_page_tables[pd_index][pt_index] = 0;
        
        // Flush the TLB for this address
        __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
    }
}

void vmm_map_page(uint32_t physical_addr, uint32_t virtual_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FFU;
    
    // Check if this directory entry is blank/not present
    if (!(page_directory[pd_index] & VM_PRESENT)) {
        // Link the directory entry to the pre-reserved static page table and mark it present
        page_directory[pd_index] = ((uint32_t)&static_page_tables[pd_index]) | VM_PRESENT | VM_READWRITE;
    }
    
    // Update the table entry directly via its pointer
    static_page_tables[pd_index][pt_index] = (physical_addr & ~0xFFFU) | flags;
    
    // Flush the TLB
    __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void vmm_map_hardware_region(uint32_t phys_addr, uint32_t virt_addr, uint32_t size, uint32_t flags) {
    uint32_t page_offset = phys_addr & (PAGE_SIZE - 1);
    uint32_t start_phys  = phys_addr & ~(PAGE_SIZE - 1);
    uint32_t start_virt  = virt_addr & ~(PAGE_SIZE - 1);
    
    uint32_t total_size = size + page_offset;
    uint32_t num_pages = (total_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    
    // Map pages to kernel space
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t current_phys = start_phys + (i * PAGE_SIZE);
        uint32_t current_virt = start_virt + (i * PAGE_SIZE);
        
        vmm_map_page(current_phys, current_virt, flags);
    }
}

void* vmm_map_mmio_region(uint32_t phys_addr, uint32_t size_bytes, uint32_t flags) {
    uint32_t page_offset = phys_addr & (PAGE_SIZE - 1);
    uint32_t start_phys  = phys_addr & ~(PAGE_SIZE - 1);
    uint32_t total_size  = size_bytes + page_offset;
    uint32_t num_pages   = (total_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    
    uint32_t irq_flags = irq_save();
    
    // Allocate a chunk of virtual address space inside VM_START -> VM_END
    int virt_start_page = find_contiguous_bits(virt_bitmap, VIRT_BITMAP_SIZE, num_pages);
    if (virt_start_page == -1) {
        irq_restore(irq_flags);
        return NULL; 
    }
    
    uint32_t start_vaddr = VM_START + (virt_start_page * PAGE_SIZE);
    
    // Map the virtual pages explicitly to the hardware BAR frames
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t current_phys = start_phys + (i * PAGE_SIZE);
        uint32_t current_virt = start_vaddr + (i * PAGE_SIZE);
        
        vmm_map_page(current_phys, current_virt, flags);
        
        // Track the virtual pages as allocated in your bitmap tracker
        size_t bit = (size_t)virt_start_page + i;
        virt_bitmap[bit / 8] |= (1U << (bit % 8));
    }
    
    irq_restore(irq_flags);
    return (void*)(start_vaddr + page_offset);
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

uint32_t vmm_get_phys_addr(void* virtual_addr) {
    uint32_t vaddr = (uint32_t)virtual_addr;
    
    // Extract the page directory (top 10 bits) and page table (middle 10 bits) indexes
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FFU;
    
    // Extract the lower 12 bits (the offset within the 4KB page)
    uint32_t page_offset = vaddr & 0xFFFU;
    
    // Check if the page directory entry is present
    if (!(page_directory[pd_index] & VM_PRESENT)) 
        return 0;
    
    // Lookup the page table entry from your static multi-dimensional array
    uint32_t pte = static_page_tables[pd_index][pt_index];
    
    // Check if the individual page is actually marked present
    if (!(pte & VM_PRESENT)) 
        return 0;
    
    // Mask out the page flags to get the base physical frame address, 
    // then append the original page offset.
    return (pte & ~0xFFFU) + page_offset;
}

void* vmm_get_virt_addr(uint32_t physical_addr) {
    uint32_t target_frame = physical_addr & ~0xFFFU;
    uint32_t page_offset  = physical_addr & 0xFFFU;
    
    // Scan through the page directory
    for (uint32_t pd_index = 0; pd_index < VM_PAGE_DIR_SIZE; pd_index++) {
        // Check if this page table is present
        if (page_directory[pd_index] & VM_PRESENT) {
            
            // Check through this specific page table of entries
            for (uint32_t pt_index = 0; pt_index < VM_PAGE_TABLE_SIZE; pt_index++) {
                uint32_t pte = static_page_tables[pd_index][pt_index];
                
                // If the page is present and matches our target physical frame
                if ((pte & VM_PRESENT) && ((pte & ~0xFFFU) == target_frame)) {
                    
                    // Reconstruct the virtual address from the indices and offset
                    uint32_t virtual_addr = (pd_index << 22) | (pt_index << 12) | page_offset;
                    return (void*)virtual_addr;
                }
            }
        }
    }
    
    // Return NULL if no virtual mapping exists for this physical address
    return NULL;
}
