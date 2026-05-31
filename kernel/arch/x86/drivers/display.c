#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/drivers/char_rom.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>

#include <kernel/console/print.h>
#include <kernel/console/display.h>

#include <kernel/util/string.h>

#define FONT_WIDTH  5
#define FONT_HEIGHT 8

uint8_t cursor_position;
uint8_t cursor_line;

uint32_t display_width;
uint32_t display_height;

uint32_t console_width;
uint32_t console_height;

uint8_t console_glyph_width = 6;

extern struct MultibootInfo* vinfo;

extern uint32_t foreground_color;
extern uint32_t background_color;
extern uint32_t transparent_color;

uint32_t make_color(float a, float r, float g, float b);
void draw_glyph(const uint8_t* glyph_map, int glyph_index, int x, int y, uint32_t fg_color, uint32_t bg_color, int transparent_bg);
void draw_rect(int x, int y, int width, int height, uint32_t color);
void draw_rect_filled(int x, int y, int width, int height, uint32_t color);

void display_init(void) {
    display_width  = vinfo->framebuffer_width;
    display_height = vinfo->framebuffer_height;
    
    console_width  = (display_width / 5) - 16;
    console_height = (display_height / 8) - 12;
    
    foreground_color = 0xFFCACACA;
    background_color = 0xFF0A0A0A;
    transparent_color = make_color(0, 1, 1, 1);
    
    cursor_line = 0;
    cursor_position = 0;
}

uint32_t display_get_device_address(void) {
    return 0;
}

void display_busy_wait(void) {}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_height;
}

uint16_t display_get_rows(void) {
    return console_width;
}

uint16_t display_get_columbs(void) {
    return console_height;
}

void display_putc(const char ch) {
    draw_rect_filled(cursor_position * 6, cursor_line * 9, 6, 8, background_color);
    draw_glyph(char_rom, ch, cursor_position * 6, cursor_line * 9, foreground_color, background_color, transparent_color);
}

void display_newline(void) {
    uint8_t* fb_bytes = (uint8_t*)((uintptr_t)vinfo->framebuffer_addr);
    
    uint32_t pitch = vinfo->framebuffer_pitch;
    uint32_t height = display_get_height();
    
    size_t bytes_per_text_row = FONT_HEIGHT * pitch; 
    size_t bytes_to_move = (height - FONT_HEIGHT) * pitch;
    
    memmove(fb_bytes, fb_bytes + bytes_per_text_row, bytes_to_move);
    
    for (uint32_t y = height - FONT_HEIGHT; y < height; y++) {
        uint32_t* row_pixels = (uint32_t*)(fb_bytes + (y * pitch));
        for (uint32_t x = 0; x < display_get_width(); x++) {
            row_pixels[x] = background_color;
        }
    }
}

void display_cursor_set_position(uint16_t position) {
    cursor_position = (uint8_t)position;
}

void display_cursor_set_line(uint16_t line) {
    if (line >= (console_width * console_glyph_width)) line = (console_width * console_glyph_width) - 1;
    cursor_line = (uint8_t)line;
}

uint16_t display_cursor_get_position(void) {
    return cursor_position;
}

uint16_t display_cursor_get_line(void) {
    return cursor_line;
}

void display_clear(void) {
    draw_rect_filled(0, 0, display_width, display_height, background_color);
}

void console_set_blink_rate(uint8_t rate) {
}
