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

inline void draw_pixel(int x, int y, uint32_t color) {
    extern struct MultibootInfo* vinfo;
    extern uint32_t* back_buffer;
    uint32_t stride = vinfo->framebuffer_pitch / 4;
    back_buffer[y * stride + x] = color;
}

void draw_set_glyph_scheme(uint32_t foreground, uint32_t background, uint32_t transparent);

void draw_set_clip_rect(int32_t x, int32_t y, int32_t w, int32_t h);
void draw_set_base_offset(int32_t x, int32_t y);

void draw_set_info(uint32_t mb_info);
void draw_set_frame_buffer(uint32_t* buffer_ptr);
void draw_flush_region(int x, int y, int width, int height);

uint32_t make_color(float a, float r, float g, float b);

void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_rect(int x, int y, int width, int height, uint32_t color);
void draw_rect_filled(int x, int y, int width, int height, uint32_t color);
void draw_rect_gradient_horizontal(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to);
void draw_rect_gradient_vertical(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to);
void draw_rect_gradient_vertical_offset(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to, int gradient_offset);

void draw_circle(int xc, int yc, int r, uint32_t color);

void draw_glyph(const uint8_t* glyph_map, int glyph_index, int x, int y, uint32_t fg_color, uint32_t bg_color, int transparent_bg);
void draw_sprite(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey);

#endif
