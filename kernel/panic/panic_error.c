#include <kernel/panic/panic_error.h>
#include <stdint.h>

// Layout & Styling Constants
#define COLOR_BG        0xFF3A0000  // Deep dark red
#define COLOR_FG        0xFFD0D0D0  // Light grey
#define COLOR_WHITE     0xFFFFFFFF  // Pure white

#define SCALE_FACTOR    2           // 2x scaling (Change to 3, 4, etc. as needed)

#define GLYPH_WIDTH     6
#define GLYPH_HEIGHT    8

// Automatically scaled metrics
#define SCALED_WIDTH    (GLYPH_WIDTH * SCALE_FACTOR)
#define SCALED_HEIGHT   (GLYPH_HEIGHT * SCALE_FACTOR)
#define LINE_HEIGHT     (SCALED_HEIGHT + (4 * SCALE_FACTOR)) // Font height + scaled padding
#define PADDING_START   24

extern struct MultibootInfo* vinfo;
extern uint32_t* front_buffer;
extern const uint8_t char_rom[];

// Simplifies passing text rendering configurations
typedef struct {
    int x;
    int y;
    int start_x;
    uint32_t fg;
    uint32_t bg;
} TextContext;

// Forward Declarations
void iso_draw_char(char ch, int x, int y, uint32_t fg_color, uint32_t bg_color);
void iso_print(TextContext* ctx, const char* str);
void iso_print_hex32(TextContext* ctx, uint32_t val);

static void clear_screen(uint32_t color) {
    uint32_t screen_w = vinfo->framebuffer_width;
    uint32_t screen_h = vinfo->framebuffer_height;
    uint32_t stride   = vinfo->framebuffer_pitch / 4;
    
    for (uint32_t y = 0; y < screen_h; y++) {
        for (uint32_t x = 0; x < screen_w; x++) {
            front_buffer[y * stride + x] = color;
        }
    }
}

void kernel_crashout(uint32_t error_code, uint32_t faulting_address, uint8_t type, const char* extra) {
    clear_screen(COLOR_BG);
    
    TextContext ctx = {
        .x       = PADDING_START,
        .y       = PADDING_START,
        .start_x = PADDING_START,
        .fg      = COLOR_FG,
        .bg      = COLOR_BG
    };
    
    // Header
    ctx.fg = COLOR_WHITE;
    iso_print(&ctx, "            !!! FATAL KERNEL ERROR !!!\n");
    ctx.fg = COLOR_FG;
    iso_print(&ctx, "======================================================\n\n");
    
    // Process Fault Details
    switch (type) {
        case PT_GENERAL_PROTECTION_FAULT:   iso_print(&ctx, "   GENERAL PROTECTION FAULT\n"); break;
        case PT_PAGE_FAULT:                 iso_print(&ctx, "   PAGE FAULT\n"); break;
        case PT_SEG_FAULT:                  iso_print(&ctx, "   SEGMENTATION FAULT\n"); break;
        case PT_OUT_OF_MEMORY:              iso_print(&ctx, "   OUT-OF-MEMORY\n"); break;
        default:                            iso_print(&ctx, "   UNKNOWN KERNEL FAULT\n"); break;
    }
    
    //
    // General & page faults
    
    if (type == PT_GENERAL_PROTECTION_FAULT || type == PT_PAGE_FAULT) {
        iso_print(&ctx, "\n\n ACCESS VIOLATION: ");
        iso_print(&ctx, (error_code & PF_WRITE) ? "[WRITE] @ " : "[READ] @ ");
        
        iso_print(&ctx, "0x");
        iso_print_hex32(&ctx, faulting_address);
        iso_print(&ctx, "\n\n");
        
        iso_print(&ctx, (error_code & PF_USER) ? " User space\n" : " Kernel space (Supervisor)\n");
        iso_print(&ctx, "\n");
        iso_print(&ctx, (error_code & PF_PRESENT) ? " Page-protection violation\n" : " Page not present violation\n");
        
    } else 
    
    //
    // Signal segmentation violation
    
    if (type == PT_SEG_FAULT) {
        iso_print(&ctx, "\n\n ACCESS VIOLATION: ");
        iso_print(&ctx, (error_code & PF_WRITE) ? "[WRITE] @ " : "[READ] @ ");
        
        iso_print(&ctx, "0x");
        iso_print_hex32(&ctx, faulting_address);
        iso_print(&ctx, "\n\n");
        
        iso_print(&ctx, (error_code & PF_USER) ? " User space\n" : " Kernel space (Supervisor)\n");
        iso_print(&ctx, "\n");
        
    } else 
    
    //
    // Out of memory
    
    if (type == PT_OUT_OF_MEMORY) {
        iso_print(&ctx, "\n\n ALLOCATION FAILURE\n\n");
        
        if (extra) iso_print(&ctx, extra);
        
    }
    
    // Force write memory fence ensuring pixels have hit VRAM
    __asm__ volatile("sfence" ::: "memory");
    
    // Halt execution safely
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void iso_draw_char(char ch, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    uint32_t stride   = vinfo->framebuffer_pitch / 4;
    uint32_t screen_w = vinfo->framebuffer_width;
    uint32_t screen_h = vinfo->framebuffer_height;
    
    uint32_t rom_index = (uint32_t)ch * GLYPH_WIDTH;
    
    for (int col = 0; col < GLYPH_WIDTH; col++) {
        uint8_t column_mask = char_rom[rom_index + col];
        
        for (int row = 0; row < GLYPH_HEIGHT; row++) {
            uint32_t color = (column_mask & (1 << row)) ? fg_color : bg_color;
            
            // Map a single font bit into a larger block of screen pixels
            for (int sy = 0; sy < SCALE_FACTOR; sy++) {
                int pixel_y = y + (row * SCALE_FACTOR) + sy;
                if (pixel_y < 0 || pixel_y >= (int)screen_h) continue;
                
                for (int sx = 0; sx < SCALE_FACTOR; sx++) {
                    int pixel_x = x + (col * SCALE_FACTOR) + sx;
                    if (pixel_x < 0 || pixel_x >= (int)screen_w) continue;
                    
                    front_buffer[pixel_y * stride + pixel_x] = color;
                }
            }
        }
    }
}

void iso_print(TextContext* ctx, const char* str) {
    while (*str) {
        if (*str == '\n') {
            ctx->y += LINE_HEIGHT;
            ctx->x = ctx->start_x;
        } else {
            iso_draw_char(*str, ctx->x, ctx->y, ctx->fg, ctx->bg);
            ctx->x += SCALED_WIDTH; // Advance cursor by scaled width
        }
        str++;
    }
}

void iso_print_hex32(TextContext* ctx, uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    buffer[8] = '\0';
    
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    
    iso_print(ctx, buffer);
}
