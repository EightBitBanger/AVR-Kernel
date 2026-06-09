#ifndef DRAWING_2D_H
#define DRAWING_2D_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>

#define COLOR(val) (uint16_t)((val) & 0xFFFF), (uint16_t)((val) >> 16)

struct ClippingPlane {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
};

inline uint32_t blend_pixels(uint32_t src, uint32_t dest) {
    uint8_t alpha = (src >> 24) & 0xFF;
    
    if (alpha == 0)   return dest;
    if (alpha == 255) return src;
    
    uint32_t src_r = (src >> 16) & 0xFF;
    uint32_t src_g = (src >> 8)  & 0xFF;
    uint32_t src_b = src         & 0xFF;
    
    uint32_t dest_r = (dest >> 16) & 0xFF;
    uint32_t dest_g = (dest >> 8)  & 0xFF;
    uint32_t dest_b = dest         & 0xFF;
    
    uint32_t out_r = dest_r + ((src_r - dest_r) * alpha) / 255;
    uint32_t out_g = dest_g + ((src_g - dest_g) * alpha) / 255;
    uint32_t out_b = dest_b + ((src_b - dest_b) * alpha) / 255;
    
    return (0xFF000000) | (out_r << 16) | (out_g << 8) | out_b;
}

inline void draw_pixel(int x, int y, uint32_t color) {
    extern uint32_t* frame_buffer;
    extern uint32_t buffer_stride;
    
    size_t offset = (size_t)y * buffer_stride + x;
    uint8_t alpha = (color >> 24) & 0xFF;
    
    frame_buffer[offset] = color;
}

void draw_set_glyph_scheme(uint32_t foreground, uint32_t background, uint32_t transparent);

void draw_set_clip_rect(int32_t x, int32_t y, int32_t w, int32_t h);
void draw_set_origin(int32_t x, int32_t y);

void draw_set_buffer_default(void);
void draw_set_buffer(uint32_t* target_buffer, uint32_t width, uint32_t height);

void draw_set_info(uint32_t mbi);
void draw_set_frame_front_buffer(uint32_t buffer_ptr);
void draw_set_frame_back_buffer(uint32_t buffer_ptr);

void draw_flush_region(int x, int y, int width, int height);
void draw_flush_display(void);

uint32_t make_color(float a, float r, float g, float b);

void draw_line(int x0, int y0, int x1, int y1, uint32_t color);

void draw_rect(int x, int y, int width, int height, uint32_t color);
void draw_rect_filled(int x, int y, int width, int height, uint32_t color);
void draw_rect_filled_blend(int x, int y, int width, int height, uint32_t color);
void draw_rect_gradient_vertical_blend(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to);

void draw_circle(int xc, int yc, int r, uint32_t color);

void draw_text(int16_t x, int16_t y, const char* text, uint32_t color);

void draw_glyph(const uint8_t* glyph_map, int glyph_index, int x, int y, uint32_t fg_color, uint32_t bg_color, int transparent_bg);
void draw_sprite(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey);
void draw_sprite_blend(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey);

#endif
