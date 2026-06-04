#include <kernel/panic/panic_error.h>

extern struct MultibootInfo* vinfo;
extern const uint8_t char_rom[];

// Emergency isolated hyper simple text drawing

void iso_draw_char(char ch, int x, int y, uint32_t fg_color, uint32_t bg_color);
void iso_print(int* x, int* y, int start_x, const char* str, uint32_t fg_color, uint32_t bg_color);
void iso_print_hex32(int* x, int* y, int start_x, uint32_t val, uint32_t fg_color, uint32_t bg_color);

void kernel_crashout(uint32_t error_code, uint32_t faulting_address, uint8_t type, const char* extra) {
    uint32_t colorbg = 0xFF3A0000;
    uint32_t colorfg = 0xFFD0D0D0;
    uint32_t colorwhite = 0xFFFFFFFF;
    
    uint32_t screen_w = vinfo->framebuffer_width;
    uint32_t screen_h = vinfo->framebuffer_height;
    uint32_t stride = vinfo->framebuffer_pitch / 4;
    uint32_t* fb = (uint32_t*)(uintptr_t)vinfo->framebuffer_addr;
    
    // Clear background
    for (uint32_t y = 0; y < screen_h; y++) {
        for (uint32_t x = 0; x < screen_w; x++) {
            fb[y * stride + x] = colorbg;
        }
    }
    
    int start_x = 24;
    int cur_x = start_x;
    int cur_y = 24;
    
    iso_print(&cur_x, &cur_y, start_x, "             !!! FATAL KERNEL ERROR !!!\n", colorwhite, colorbg);
    iso_print(&cur_x, &cur_y, start_x, "======================================================\n\n", colorfg, colorbg);
    
    if (type < 0x03) {
        if (type == 0x00) iso_print(&cur_x, &cur_y, start_x, "   GENERAL PROTECTION FAULT\n", colorfg, colorbg);
        if (type == 0x01) iso_print(&cur_x, &cur_y, start_x, "   PAGE FAULT\n", colorfg, colorbg);
        if (type == 0x02) iso_print(&cur_x, &cur_y, start_x, "   SEGMENTATION FAULT\n", colorfg, colorbg);
        
        iso_print(&cur_x, &cur_y, start_x, "\n\n Access violation: ", colorfg, colorbg);
        
        if (error_code & PF_WRITE) {
            iso_print(&cur_x, &cur_y, start_x, "[WRITE] @ ", colorfg, colorbg);
        } else {
            iso_print(&cur_x, &cur_y, start_x, "[READ] @ ", colorfg, colorbg);
        }
        
        iso_print(&cur_x, &cur_y, start_x, "0x", colorfg, colorbg);
        iso_print_hex32(&cur_x, &cur_y, start_x, faulting_address, colorfg, colorbg);
        iso_print(&cur_x, &cur_y, start_x, "\n\n", colorfg, colorbg);
        
        if (error_code & PF_USER) {
            iso_print(&cur_x, &cur_y, start_x, " User space\n", colorfg, colorbg);
        } else {
            iso_print(&cur_x, &cur_y, start_x, " Kernel space (Supervisor)\n", colorfg, colorbg);
        }
        
        iso_print(&cur_x, &cur_y, start_x, "\n", colorfg, colorbg);
        
        if (error_code & PF_PRESENT) {
            iso_print(&cur_x, &cur_y, start_x, " Page-protection violation\n", colorfg, colorbg);
        } else {
            iso_print(&cur_x, &cur_y, start_x, " Page not present violation\n", colorfg, colorbg);
        }
    }
    
    // Kernel level out of memory
    
    if (type == 0x03) {
        iso_print(&cur_x, &cur_y, start_x, "   OUT-OF-MEMORY\n", colorfg, colorbg);
        
        iso_print(&cur_x, &cur_y, start_x, "\n\n Allocation failure: ", colorfg, colorbg);
        
        iso_print(&cur_x, &cur_y, start_x, extra, colorfg, colorbg);
        iso_print(&cur_x, &cur_y, start_x, "\n\n", colorfg, colorbg);
        
        iso_print(&cur_x, &cur_y, start_x, " Kernel space (Supervisor)\n", colorfg, colorbg);
        
    }
    
    // Force an out-of-order write memory fence ensuring pixels have hit VRAM
    __asm__ volatile("sfence" ::: "memory");
    
    // Halt the system
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}

void iso_draw_char(char ch, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    uint32_t* fb = (uint32_t*)(uintptr_t)vinfo->framebuffer_addr;
    uint32_t stride = vinfo->framebuffer_pitch / 4;
    uint32_t screen_w = vinfo->framebuffer_width;
    uint32_t screen_h = vinfo->framebuffer_height;
    
    const int glyph_width = 6;
    const int glyph_height = 8;
    uint32_t rom_index = (uint32_t)ch * glyph_width;
    
    for (int col = 0; col < glyph_width; col++) {
        int pixel_x = x + col;
        if (pixel_x < 0 || pixel_x >= (int)screen_w) continue;
        
        uint8_t column_mask = char_rom[rom_index + col];
        
        for (int row = 0; row < glyph_height; row++) {
            int pixel_y = y + row;
            if (pixel_y < 0 || pixel_y >= (int)screen_h) continue;
            
            if (column_mask & (1 << row)) {
                fb[pixel_y * stride + pixel_x] = fg_color;
            } else {
                fb[pixel_y * stride + pixel_x] = bg_color;
            }
        }
    }
}

void iso_print(int* x, int* y, int start_x, const char* str, uint32_t fg_color, uint32_t bg_color) {
    const int glyph_width = 6;
    const int line_height = 12; // 8-pixel font + 4-pixel padding safety line space
    
    while (*str) {
        if (*str == '\n') {
            *y += line_height;
            *x = start_x;
        } else {
            iso_draw_char(*str, *x, *y, fg_color, bg_color);
            *x += glyph_width;
        }
        str++;
    }
}

void iso_print_hex32(int* x, int* y, int start_x, uint32_t val, uint32_t fg_color, uint32_t bg_color) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    buffer[8] = '\0';
    
    // Parse bytes out backwardly 
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    
    iso_print(x, y, start_x, buffer, fg_color, bg_color);
}

void kernel_test_trigger_page_fault(void) {
    // 0x40000000 = 1GB which is well above the 64 MB (0x04000000) mapped region
    // The page directory entry for this address will have the Present bit (VM_PRESENT) set to 0.
    volatile uint32_t* unmapped_address = (volatile uint32_t*)0x40000000;
    
    // Attempting to read from or write to this unmapped address will force an architectural Page Fault (#PF)
    uint32_t crash_trigger = *unmapped_address;
    
    // Prevent the compiler from optimizing away the read operation
    (void)crash_trigger; 
}
