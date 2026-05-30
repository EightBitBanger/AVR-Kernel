#include <kernel/util/math.h>
#include <kernel/arch/x86/drivers/draw.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/util/string.h>

#define CLIP_INSIDE  0
#define CLIP_LEFT    1
#define CLIP_RIGHT   2
#define CLIP_BOTTOM  4
#define CLIP_TOP     8

struct MultibootInfo* vinfo;

extern uint32_t display_width;
extern uint32_t display_height;

uint32_t* front_buffer = NULL;
uint32_t* back_buffer = NULL;

int32_t display_base_x = 0;
int32_t display_base_y = 0;

uint32_t* frame_buffer = NULL;
uint32_t buffer_width = 0;
uint32_t buffer_height = 0;

uint32_t buffer_stride = 0;

// Console text colors
uint32_t foreground_color;
uint32_t background_color;
uint32_t transparent_color;

struct ClippingPlane clipping_plain;

void draw_set_glyph_scheme(uint32_t foreground, uint32_t background, uint32_t transparent) {
    foreground_color  = foreground;
    background_color  = background;
    transparent_color = transparent;
}

void draw_set_base_offset(int32_t x, int32_t y) {
    display_base_x = x;
    display_base_y = y;
}

void draw_set_buffer_default(void) {
    buffer_width = display_width;
    buffer_height = display_height;
    buffer_stride = vinfo->framebuffer_pitch / 4; 
    frame_buffer = back_buffer;
}

void draw_set_buffer(uint32_t* target_buffer, uint32_t width, uint32_t height) {
    buffer_width = width;
    buffer_height = height;
    buffer_stride = width;
    frame_buffer = target_buffer;
}

void draw_set_clip_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    clipping_plain.min_x = x;
    clipping_plain.min_y = y;
    clipping_plain.max_x = x + w;
    clipping_plain.max_y = y + h;
}

uint32_t make_color(float a, float r, float g, float b) {
    uint32_t a_int = (uint32_t)(clamp(a, 0.0f, 1.0f) * 255.0f);
    uint32_t r_int = (uint32_t)(clamp(r, 0.0f, 1.0f) * 255.0f);
    uint32_t g_int = (uint32_t)(clamp(g, 0.0f, 1.0f) * 255.0f);
    uint32_t b_int = (uint32_t)(clamp(b, 0.0f, 1.0f) * 255.0f);
    
    return (a_int << 24) | (r_int << 16) | (g_int << 8) | b_int;
}

void draw_set_info(uint32_t mb_info) {
    vinfo = (struct MultibootInfo*)(uintptr_t)mb_info;
    front_buffer = (uint32_t*)(uintptr_t)vinfo->framebuffer_addr;
}

void draw_set_frame_buffer(uint32_t* buffer_ptr) {
    back_buffer = buffer_ptr;
}

void draw_flush_region(int x, int y, int width, int height) {
    if (x >= (int)display_width || y >= (int)display_height || (x + width) <= 0 || (y + height) <= 0) 
        return;
    if (!back_buffer || !front_buffer) 
        return;
    
    int x_start = (x < 0) ? 0 : x;
    int y_start = (y < 0) ? 0 : y;
    int x_end = (x + width > (int)display_width) ? (int)display_width : (x + width);
    int y_end = (y + height > (int)display_height) ? (int)display_height : (y + height);
    
    uint32_t hw_stride = vinfo->framebuffer_pitch / 4;
    uint32_t back_stride = display_width; 
    int pixels_to_copy = (x_end - x_start);
    
    if (pixels_to_copy <= 0) return;

    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        size_t src_offset = (curr_y * back_stride) + x_start;
        size_t dest_offset = (curr_y * hw_stride) + x_start;
        
        uint32_t* dst = &front_buffer[dest_offset];
        uint32_t* src = &back_buffer[src_offset];
        int rem_pixels = pixels_to_copy;

        // Safe Unrolled Burst Loop: Works perfectly with Write-Combining
        // Reads 16 bytes straight from cached RAM registers, then dumps them sequentially into VRAM
        while (rem_pixels >= 4) {
            uint32_t r0, r1, r2, r3;

            asm volatile(
                "movl 0(%4), %0\n\t"
                "movl 4(%4), %1\n\t"
                "movl 8(%4), %2\n\t"
                "movl 12(%4), %3\n\t"
                
                "movl %0, 0(%5)\n\t"
                "movl %1, 4(%5)\n\t"
                "movl %2, 8(%5)\n\t"
                "movl %3, 12(%5)\n\t"
                : "=&r"(r0), "=&r"(r1), "=&r"(r2), "=&r"(r3) // Let the compiler choose temporary registers safely
                : "r"(src), "r"(dst)
                : "memory"
            );

            dst += 4;
            src += 4;
            rem_pixels -= 4;
        }

        // Clean up remaining trailing pixels (1-3 pixels)
        while (rem_pixels > 0) {
            *dst = *src;
            dst++;
            src++;
            rem_pixels--;
        }
    }
}

static inline int render_clip_compute_outcode(int x, int y) {
    int code = CLIP_INSIDE;
         if (x < clipping_plain.min_x)  code |= CLIP_LEFT;
    else if (x >= clipping_plain.max_x) code |= CLIP_RIGHT;
         if (y < clipping_plain.min_y)  code |= CLIP_TOP;
    else if (y >= clipping_plain.max_y) code |= CLIP_BOTTOM;
    return code;
}

static inline void render_circle_pixel(int sx, int sy, uint32_t c) {
    if (sx >= clipping_plain.min_x && sx < clipping_plain.max_x &&
        sy >= clipping_plain.min_y && sy < clipping_plain.max_y) {
        draw_pixel(sx, sy, c);
    }
}


void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    x0 += display_base_x; y0 += display_base_y;
    x1 += display_base_x; y1 += display_base_y;
    
    int code0 = render_clip_compute_outcode(x0, y0);
    int code1 = render_clip_compute_outcode(x1, y1);
    bool accept = false;
    
    while (true) {
        if ((code0 | code1) == 0) { accept = true; break; }
        else if (code0 & code1) { break; }
        else {
            int x, y;
            int outcodeOut = code0 ? code0 : code1;
            
            if (outcodeOut & CLIP_TOP) {
                x = x0 + (x1 - x0) * (clipping_plain.min_y - y0) / (y1 - y0);
                y = clipping_plain.min_y;
            } else if (outcodeOut & CLIP_BOTTOM) {
                x = x0 + (x1 - x0) * (clipping_plain.max_y - 1 - y0) / (y1 - y0);
                y = clipping_plain.max_y - 1;
            } else if (outcodeOut & CLIP_RIGHT) {
                y = y0 + (y1 - y0) * (clipping_plain.max_x - 1 - x0) / (x1 - x0);
                x = clipping_plain.max_x - 1;
            } else if (outcodeOut & CLIP_LEFT) {
                y = y0 + (y1 - y0) * (clipping_plain.min_x - x0) / (x1 - x0);
                x = clipping_plain.min_x;
            }
            
            if (outcodeOut == code0) {
                x0 = x; y0 = y;
                code0 = render_clip_compute_outcode(x0, y0);
            } else {
                x1 = x; y1 = y;
                code1 = render_clip_compute_outcode(x1, y1);
            }
        }
    }
    
    if (!accept) return;
    
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}


void draw_rect(int x, int y, int width, int height, uint32_t color) {
    x += display_base_x; y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end) return;
    
    // Top & Bottom horizontal line optimizations
    if (y >= clipping_plain.min_y && y < clipping_plain.max_y) {
        for (int cx = x_start; cx < x_end; cx++) draw_pixel(cx, y, color);
    }
    int bottom_y = y + height - 1;
    if (bottom_y >= clipping_plain.min_y && bottom_y < clipping_plain.max_y) {
        for (int cx = x_start; cx < x_end; cx++) draw_pixel(cx, bottom_y, color);
    }
    // Left & Right vertical lines
    if (x >= clipping_plain.min_x && x < clipping_plain.max_x) {
        for (int cy = y_start; cy < y_end; cy++) draw_pixel(x, cy, color);
    }
    int right_x = x + width - 1;
    if (right_x >= clipping_plain.min_x && right_x < clipping_plain.max_x) {
        for (int cy = y_start; cy < y_end; cy++) draw_pixel(right_x, cy, color);
    }
}

void draw_rect_filled_blend(int x, int y, int width, int height, uint32_t color) {
    uint32_t alpha = (color >> 24) & 0xFF;
    if (alpha == 0) return; 
    
    x += display_base_x; y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end) return; 
    int width_to_draw = x_end - x_start;
    
    if (alpha == 255) {
        for (int curr_y = y_start; curr_y < y_end; curr_y++) {
            uint32_t* dest = &frame_buffer[(curr_y * buffer_stride) + x_start];
            for (int i = 0; i < width_to_draw; i++) {
                dest[i] = color;
            }
        }
    } else {
        for (int curr_y = y_start; curr_y < y_end; curr_y++) {
            uint32_t* dest = &frame_buffer[(curr_y * buffer_stride) + x_start];
            for (int i = 0; i < width_to_draw; i++) {
                dest[i] = blend_pixels(color, dest[i]);
            }
        }
    }
}

void draw_rect_filled(int x, int y, int width, int height, uint32_t color) {
    x += display_base_x; y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end) return; 
    size_t dwords_to_write = (size_t)(x_end - x_start);
    
    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        uint32_t* dest = &frame_buffer[(curr_y * buffer_stride) + x_start];
        size_t count = dwords_to_write; // Copy bounds locally so it isn't cleared to 0
        
        asm volatile(
            "cld\n\t"
            "rep stosl"
            : "+D"(dest), "+c"(count)
            : "a"(color)
            : "memory"
        );
    }
}

void draw_rect_gradient_vertical_blend(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to) {
    int screen_y_start = y + display_base_y;
    x += display_base_x; y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end || height <= 1) return;
    
    int a1 = (color_from >> 24) & 0xFF, r1 = (color_from >> 16) & 0xFF, g1 = (color_from >> 8) & 0xFF, b1 = color_from & 0xFF;
    int a2 = (color_to >> 24)   & 0xFF, r2 = (color_to >> 16)   & 0xFF, g2 = (color_to >> 8)   & 0xFF, b2 = color_to   & 0xFF;
    
    int div = height - 1;
    int a_step = ((a2 - a1) << 16) / div;
    int r_step = ((r2 - r1) << 16) / div;
    int g_step = ((g2 - g1) << 16) / div;
    int b_step = ((b2 - b1) << 16) / div;
    
    int initial_offset = y_start - screen_y_start;
    int cur_a = (a1 << 16) + (a_step * initial_offset);
    int cur_r = (r1 << 16) + (r_step * initial_offset);
    int cur_g = (g1 << 16) + (g_step * initial_offset);
    int cur_b = (b1 << 16) + (b_step * initial_offset);
    
    int width_to_draw = x_end - x_start;
    
    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        uint8_t a = (uint8_t)(cur_a >> 16);
        uint8_t r = (uint8_t)(cur_r >> 16);
        uint8_t g = (uint8_t)(cur_g >> 16);
        uint8_t b = (uint8_t)(cur_b >> 16);
        
        uint32_t final_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        uint32_t* dest = &frame_buffer[(curr_y * buffer_stride) + x_start];
        
        for (int i = 0; i < width_to_draw; i++) {
            dest[i] = blend_pixels(final_color, dest[i]);
        }
        
        cur_a += a_step; cur_r += r_step; cur_g += g_step; cur_b += b_step;
    }
}

void draw_circle(int xc, int yc, int r, uint32_t color) {
    xc += display_base_x; yc += display_base_y;
    
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    
    while (y >= x) {
        render_circle_pixel(xc + x, yc + y, color);
        render_circle_pixel(xc - x, yc + y, color);
        render_circle_pixel(xc + x, yc - y, color);
        render_circle_pixel(xc - x, yc - y, color);
        render_circle_pixel(xc + y, yc + x, color);
        render_circle_pixel(xc - y, yc + x, color);
        render_circle_pixel(xc + y, yc - x, color);
        render_circle_pixel(xc - y, yc - x, color);
        x++;
        if (d > 0) { y--; d = d + 4 * (x - y) + 10; }
        else { d = d + 4 * x + 6; }
    }
}

void draw_glyph(const uint8_t* glyph_map, int glyph_index, int x, int y, uint32_t fg_color, uint32_t bg_color, int transparent_bg) {
    x += display_base_x; y += display_base_y;
    
    const int GLYPH_W = 6;
    const int GLYPH_H = 8;
    
    int src_x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x - x : 0;
    int src_y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y - y : 0;
    int src_x_end = (x + GLYPH_W > clipping_plain.max_x) ? clipping_plain.max_x - x : GLYPH_W;
    int src_y_end = (y + GLYPH_H > clipping_plain.max_y) ? clipping_plain.max_y - y : GLYPH_H;
    
    if (src_x_start >= src_x_end || src_y_start >= src_y_end) return;
    
    const uint8_t* glyph_data = &glyph_map[glyph_index * GLYPH_W];
    for (int src_x = src_x_start; src_x < src_x_end; src_x++) {
        uint8_t column_byte = glyph_data[src_x];
        int dest_x = x + src_x;
        for (int src_y = src_y_start; src_y < src_y_end; src_y++) {
            if ((column_byte >> src_y) & 1) {
                draw_pixel(dest_x, y + src_y, fg_color);
            } else if (!transparent_bg) {
                draw_pixel(dest_x, y + src_y, bg_color);
            }
        }
    }
}

void draw_sprite(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey) {
    x += display_base_x; y += display_base_y;
    
    int src_x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x - x : 0;
    int src_y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y - y : 0;
    int src_x_end = (x + sprite_w > clipping_plain.max_x) ? clipping_plain.max_x - x : sprite_w;
    int src_y_end = (y + sprite_h > clipping_plain.max_y) ? clipping_plain.max_y - y : sprite_h;
    
    if (src_x_start >= src_x_end || src_y_start >= src_y_end) return;
    
    int width_to_copy = src_x_end - src_x_start;
    int screen_x_start = x + src_x_start;
    size_t dwords_to_copy = (size_t)width_to_copy;
    
    for (int src_y = src_y_start; src_y < src_y_end; src_y++) {
        int screen_y = y + src_y;
        
        uint32_t* dest_row = &frame_buffer[(screen_y * buffer_stride) + screen_x_start];
        const uint32_t* src_row = &sprite_data[(src_y * sprite_w) + src_x_start];
        size_t count = dwords_to_copy;
        
        asm volatile(
            "cld\n\t"
            "rep movsl"
            : "+D"(dest_row), "+S"(src_row), "+c"(count)
            :
            : "memory"
        );
    }
}

void draw_sprite_blend(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey) {
    x += display_base_x; y += display_base_y;
    
    int src_x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x - x : 0;
    int src_y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y - y : 0;
    int src_x_end = (x + sprite_w > clipping_plain.max_x) ? clipping_plain.max_x - x : sprite_w;
    int src_y_end = (y + sprite_h > clipping_plain.max_y) ? clipping_plain.max_y - y : sprite_h;
    
    if (src_x_start >= src_x_end || src_y_start >= src_y_end) return;
    
    int width_to_copy = src_x_end - src_x_start;
    int screen_x_start = x + src_x_start;
    
    for (int src_y = src_y_start; src_y < src_y_end; src_y++) {
        int screen_y = y + src_y;
        
        uint32_t* dest_row = &frame_buffer[(screen_y * buffer_stride) + screen_x_start];
        const uint32_t* src_row = &sprite_data[(src_y * sprite_w) + src_x_start];
        
        for (int i = 0; i < width_to_copy; i++) {
            uint32_t color = src_row[i];
            if (color == colorkey) continue;
            
            dest_row[i] = blend_pixels(color, dest_row[i]);
        }
    }
}
