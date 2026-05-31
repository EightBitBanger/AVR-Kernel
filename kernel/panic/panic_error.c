#include <kernel/panic/panic_error.h>

void kernel_panic_screen(uint32_t error_code, uint32_t faulting_address) {
    uint16_t display_half_width = display_get_width() / 2;
    uint16_t display_half_height = display_get_height() / 2;
    
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), 0xFF550000);
    draw_set_glyph_scheme(0xFFD0D0D0, 0xFF550000, 0xFF000000);
    
    display_cursor_set_position((display_get_width() * 2) - 8);
    
    
    print("!!! PAGE FAULT !!!\n\n");
    
    print(" Access violation: ");
    
    if (error_code & PF_WRITE) {
        print(" [WRITE] @ ");
    } else {
        print(" [READ] @ ");
    }
    
    print("0x");
    print_hex32(faulting_address);
    
    print("\n\n");
    
    // Parse and print the nature of the violation using the error code
    if (error_code & PF_PRESENT) {
        print(" Page-protection violation");
    } else {
        print(" Page not present violation");
    }
    
    print("\n\n");
    
    if (error_code & PF_USER) {
        print(" Origin: User space\n");
    } else {
        print(" Origin: Kernel space (Supervisor)\n");
    }
    
    // Flush to display
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    // Freeze system
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}
