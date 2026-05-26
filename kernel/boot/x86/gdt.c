#include <kernel/boot/x86/gdt.h>

gdt_entry_t gdt_entries[5];
gdt_ptr_t   gdt_ptr;

void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

extern void gdt_flush();

void gdt_initiate(void) {
    // Total size of GDT minus 1
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;
    
    // 1. Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);                
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);  // 2. Kernel Code: Base 0, Limit 4GB, Ring 0, Executable
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);  // 3. Kernel Data: Base 0, Limit 4GB, Ring 0, Writable
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);  // 4. User Code:   Base 0, Limit 4GB, Ring 3, Executable
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);  // 5. User Data:   Base 0, Limit 4GB, Ring 3, Writable
    
    // Inform the CPU via assembly
    gdt_flush((uint32_t)&gdt_ptr);
}

