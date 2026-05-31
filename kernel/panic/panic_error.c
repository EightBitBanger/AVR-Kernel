#include <kernel/panic/panic_error.h>

void kernel_panic_screen(uint32_t error_code, uint32_t faulting_address, uint8_t type) {
    uint32_t colorbg = 0xFF300000;
    
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), colorbg);
    draw_set_glyph_scheme(0xFFD0D0D0, colorbg, 0xFF000000);
    
    draw_text(0, 12, "             !!! FATAL KERNEL ERROR !!!", 0xFFFFFFFF);
    
    print("\n\n\n======================================================\n\n\n");
    
    if (type == 0x00) print("   GENERAL PROTECTION FAULT");
    if (type == 0x01) print("   PAGE FAULT");
    if (type == 0x02) print("   SEGMENTATION FAULT");
    
    print("\n\n\n\n");
    print(" Access violation: ");
    
    if (error_code & PF_WRITE) {
        print(" [WRITE] @ ");
    } else {
        print(" [READ] @ ");
    }
    
    print("0x");
    print_hex32(faulting_address);
    
    print("\n\n");
    
    if (error_code & PF_USER) {
        print(" User space\n");
    } else {
        print(" Kernel space (Supervisor)\n");
    }
    
    print("\n\n\n");
    
    if (error_code & PF_PRESENT) {
        print(" Page-protection violation");
    } else {
        print(" Page not present violation");
    }
    
    // Flush to display
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    // Freeze system
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}
