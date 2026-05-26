#include <kernel/util/math.h>
#include <kernel/arch/x86/drivers/draw.h>
#include <stddef.h>
#include <stdbool.h>

#define CLIP_INSIDE 0
#define CLIP_LEFT   1
#define CLIP_RIGHT  2
#define CLIP_BOTTOM 4
#define CLIP_TOP    8

struct MultibootInfo* vinfo;

extern uint32_t display_width;
extern uint32_t display_height;

uint32_t* front_buffer = NULL;
uint32_t* back_buffer  = NULL;

int32_t display_base_x = 0;
int32_t display_base_y = 0;

// Console text colors
uint32_t foreground_color;
uint32_t background_color;
uint32_t transparent_color;

struct ClippingPlane clipping_plain;

void draw_set_glyph_scheme(uint32_t foreground, uint32_t background, uint32_t transparent) {
    foreground  = foreground_color;
    background  = background_color;
    transparent = transparent_color;
}

void draw_set_base_offset(int32_t x, int32_t y) {
    display_base_x = x;
    display_base_y = y;
}

void draw_set_clip_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    clipping_plain.min_x = x;
    clipping_plain.min_y = y;
    clipping_plain.max_x = x + w;
    clipping_plain.max_y = y + h;
}

uint32_t make_color(float a, float r, float g, float b) {
    uint32_t a_int = (uint32_t)(clamp(a, 0, 1) * 255.0f);
    uint32_t r_int = (uint32_t)(clamp(r, 0, 1) * 255.0f);
    uint32_t g_int = (uint32_t)(clamp(g, 0, 1) * 255.0f);
    uint32_t b_int = (uint32_t)(clamp(b, 0, 1) * 255.0f);
    
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
    
    uint32_t stride = vinfo->framebuffer_pitch / 4;
    int words_to_copy = x_end - x_start;
    
    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        size_t buffer_offset = (curr_y * stride) + x_start;
        
        uint32_t* src = &back_buffer[buffer_offset];
        uint32_t* dest = &front_buffer[buffer_offset];
        int count = words_to_copy; 
        
        asm volatile (
            "cld;\n\t"
            "rep movsd;\n\t"
            : "+S"(src), "+D"(dest), "+c"(count)
            :
            : "memory"
        );
    }
}

static inline int render_clip_compute_outcode(int x, int y) {
    int code = CLIP_INSIDE;
    if (x < clipping_plain.min_x)       code |= CLIP_LEFT;
    else if (x >= clipping_plain.max_x) code |= CLIP_RIGHT;
    if (y < clipping_plain.min_y)       code |= CLIP_TOP;
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
    // Apply window offsets
    x0 += display_base_x;
    y0 += display_base_y;
    x1 += display_base_x;
    y1 += display_base_y;
    
    int code0 = render_clip_compute_outcode(x0, y0);
    int code1 = render_clip_compute_outcode(x1, y1);
    bool accept = false;
    
    while (true) {
        if ((code0 | code1) == 0) {
            accept = true; 
            break;
        } else if (code0 & code1) {
            break; 
        } else {
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
    
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
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
    // Apply window offsets
    x += display_base_x;
    y += display_base_y;
    
    // Top Border
    if (y >= clipping_plain.min_y && y < clipping_plain.max_y) {
        int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
        int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
        for (int cx = x_start; cx < x_end; cx++) draw_pixel(cx, y, color);
    }
    // Bottom Border
    int bottom_y = y + height - 1;
    if (bottom_y >= clipping_plain.min_y && bottom_y < clipping_plain.max_y) {
        int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
        int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
        for (int cx = x_start; cx < x_end; cx++) draw_pixel(cx, bottom_y, color);
    }
    // Left Border
    if (x >= clipping_plain.min_x && x < clipping_plain.max_x) {
        int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
        int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
        for (int cy = y_start; cy < y_end; cy++) draw_pixel(x, cy, color);
    }
    // Right Border
    int right_x = x + width - 1;
    if (right_x >= clipping_plain.min_x && right_x < clipping_plain.max_x) {
        int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
        int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
        for (int cy = y_start; cy < y_end; cy++) draw_pixel(right_x, cy, color);
    }
}

void draw_rect_filled(int x, int y, int width, int height, uint32_t color) {
    // Apply window offsets
    x += display_base_x;
    y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end) return; 
    
    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        for (int curr_x = x_start; curr_x < x_end; curr_x++) {
            draw_pixel(curr_x, curr_y, color);
        }
    }
}

void draw_rect_gradient_vertical(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to) {
    int screen_y_start = y + display_base_y;
    x += display_base_x;
    y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end || height <= 1) return;
    
    // Extract starting channels
    int a1 = (color_from >> 24) & 0xFF, r1 = (color_from >> 16) & 0xFF, g1 = (color_from >> 8) & 0xFF, b1 = color_from & 0xFF;
    int a2 = (color_to >> 24)   & 0xFF, r2 = (color_to >> 16)   & 0xFF, g2 = (color_to >> 8)   & 0xFF, b2 = color_to   & 0xFF;
    
    // Calculate the total distance changes
    int delta_a = a2 - a1;
    int delta_r = r2 - r1;
    int delta_g = g2 - g1;
    int delta_b = b2 - b1;
    
    int div = height - 1;
    
    // Render strictly within the visible (clipped) screen bounds
    for (int curr_y = y_start; curr_y < y_end; curr_y++) {
        
        // Calculate exactly which logical row of the rectangle we are on (0 to height-1)
        int logical_row = curr_y - screen_y_start;
        
        // Fixed-point scale: Calculate the exact color for this specific row.
        // We multiply the delta by logical_row first (scaled up by 16 bits), then divide by total height.
        int cur_a = (a1 << 16) + ((delta_a * logical_row) << 16) / div;
        int cur_r = (r1 << 16) + ((delta_r * logical_row) << 16) / div;
        int cur_g = (g1 << 16) + ((delta_g * logical_row) << 16) / div;
        int cur_b = (b1 << 16) + ((delta_b * logical_row) << 16) / div;
        
        // Shift right by 16 to get our true 0-255 color channels
        uint8_t a = (uint8_t)(cur_a >> 16);
        uint8_t r = (uint8_t)(cur_r >> 16);
        uint8_t g = (uint8_t)(cur_g >> 16);
        uint8_t b = (uint8_t)(cur_b >> 16);
        
        uint32_t final_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        
        for (int curr_x = x_start; curr_x < x_end; curr_x++) {
            draw_pixel(curr_x, curr_y, final_color);
        }
    }
}

void draw_rect_gradient_vertical_offset(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to, int gradient_offset) {
    // 1. Calculate standard screen coordinates and clipping bounds
    int screen_y_start = y + display_base_y;
    x += display_base_x;
    y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end || height <= 1) return;

    // 2. Extract starting channels
    int a1 = (color_from >> 24) & 0xFF, r1 = (color_from >> 16) & 0xFF, g1 = (color_from >> 8) & 0xFF, b1 = color_from & 0xFF;
    int a2 = (color_to >> 24)   & 0xFF, r2 = (color_to >> 16)   & 0xFF, g2 = (color_to >> 8)   & 0xFF, b2 = color_to   & 0xFF;
    
    // 3. Calculate fixed-point step deltas
    int div = height - 1;
    int a_step = ((a2 - a1) << 16) / div;
    int r_step = ((r2 - r1) << 16) / div;
    int g_step = ((g2 - g1) << 16) / div;
    int b_step = ((b2 - b1) << 16) / div;
    
    // 4. Initialize color positions, applying the offset to the starting color point
    int cur_a = (a1 << 16) + (a_step * gradient_offset);
    int cur_r = (r1 << 16) + (r_step * gradient_offset);
    int cur_g = (g1 << 16) + (g_step * gradient_offset);
    int cur_b = (b1 << 16) + (b_step * gradient_offset);

    // 5. Render loop
    for (int i = 0; i < height; i++) {
        int curr_y = screen_y_start + i;
        
        if (curr_y >= y_start && curr_y < y_end) {
            // Safety clamping: Ensure fixed-point values don't overflow past 0 or 255 
            // due to the offset pushing them out of bounds.
            int final_a = cur_a >> 16;
            int final_r = cur_r >> 16;
            int final_g = cur_g >> 16;
            int final_b = cur_b >> 16;

            if (final_a < 0) final_a = 0; else if (final_a > 255) final_a = 255;
            if (final_r < 0) final_r = 0; else if (final_r > 255) final_r = 255;
            if (final_g < 0) final_g = 0; else if (final_g > 255) final_g = 255;
            if (final_b < 0) final_b = 0; else if (final_b > 255) final_b = 255;
            
            uint32_t final_color = ((uint32_t)final_a << 24) | ((uint32_t)final_r << 16) | ((uint32_t)final_g << 8) | final_b;
            
            for (int curr_x = x_start; curr_x < x_end; curr_x++) {
                draw_pixel(curr_x, curr_y, final_color);
            }
        }
        
        if (curr_y >= y_end) break;

        cur_a += a_step;
        cur_r += r_step;
        cur_g += g_step;
        cur_b += b_step;
    }
}

void draw_rect_gradient_horizontal(int x, int y, int width, int height, uint32_t color_from, uint32_t color_to) {
    // Apply window offsets before caching original window 'x'
    int orig_x = x + display_base_x;
    x += display_base_x;
    y += display_base_y;
    
    int x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x : x;
    int y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y : y;
    int x_end = (x + width > clipping_plain.max_x) ? clipping_plain.max_x : x + width;
    int y_end = (y + height > clipping_plain.max_y) ? clipping_plain.max_y : y + height;
    
    if (x_start >= x_end || y_start >= y_end) return;
    
    uint8_t a1 = (color_from >> 24) & 0xFF, r1 = (color_from >> 16) & 0xFF, g1 = (color_from >> 8) & 0xFF, b1 = color_from & 0xFF;
    uint8_t a2 = (color_to >> 24)   & 0xFF, r2 = (color_to >> 16)   & 0xFF, g2 = (color_to >> 8)   & 0xFF, b2 = color_to   & 0xFF;
    
    for (int curr_x = x_start; curr_x < x_end; curr_x++) {
        // Calculate factor based on absolute screen space delta relative to offset original window x
        float t = (float)(curr_x - orig_x) / (float)(width - 1);
        
        uint8_t a = (uint8_t)(a1 + t * (a2 - a1));
        uint8_t r = (uint8_t)(r1 + t * (r2 - r1));
        uint8_t g = (uint8_t)(g1 + t * (g2 - g1));
        uint8_t b = (uint8_t)(b1 + t * (b2 - b1));
        
        uint32_t final_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        
        for (int curr_y = y_start; curr_y < y_end; curr_y++) {
            draw_pixel(curr_x, curr_y, final_color);
        }
    }
}

void draw_circle(int xc, int yc, int r, uint32_t color) {
    // Apply window offsets
    xc += display_base_x;
    yc += display_base_y;
    
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
    // Apply window offsets
    x += display_base_x;
    y += display_base_y;
    
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
        for (int src_y = src_y_start; src_y < src_y_end; src_y++) {
            int is_pixel_set = (column_byte >> src_y) & 1;
            if (is_pixel_set) {
                draw_pixel(x + src_x, y + src_y, fg_color);
            } else if (!transparent_bg) {
                draw_pixel(x + src_x, y + src_y, bg_color);
            }
        }
    }
}

void draw_sprite(const uint32_t* sprite_data, int sprite_w, int sprite_h, int x, int y, uint32_t colorkey) {
    // Apply window offsets
    x += display_base_x;
    y += display_base_y;
    
    int src_x_start = (x < clipping_plain.min_x) ? clipping_plain.min_x - x : 0;
    int src_y_start = (y < clipping_plain.min_y) ? clipping_plain.min_y - y : 0;
    int src_x_end = (x + sprite_w > clipping_plain.max_x) ? clipping_plain.max_x - x : sprite_w;
    int src_y_end = (y + sprite_h > clipping_plain.max_y) ? clipping_plain.max_y - y : sprite_h;
    
    if (src_x_start >= src_x_end || src_y_start >= src_y_end) return;
    
    for (int src_y = src_y_start; src_y < src_y_end; src_y++) {
        for (int src_x = src_x_start; src_x < src_x_end; src_x++) {
            uint32_t color = sprite_data[src_y * sprite_w + src_x];
            if (color == colorkey) continue;
            draw_pixel(x + src_x, y + src_y, color);
        }
    }
}
