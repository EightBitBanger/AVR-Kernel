#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/heap.h>

#include <kernel/kernel.h>
#include <kernel/math.h>
#include <kernel/string.h>

#include <kernel/console/print.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/console/virtual_key.h>


typedef struct {
    uint32_t flags;
    // ... other multiboot fields ...
    // Make sure bit 11 of flags is set before reading these:
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    
    // Framebuffer info (Multiboot 0.6.96+)
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} __attribute__((packed)) multiboot_info_t;


extern char _kernel_end[];


#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 200
#define VIDEO_MEMORY  ((uint8_t*)0xA0000)


#define C_BLACK                0
#define C_BLUE                 1
#define C_GREEN                2
#define C_CYAN                 3
#define C_RED                  4
#define C_MAGENTA              5
#define C_BROWN                6
#define C_GRAY                 7
#define C_DARK                 8
#define C_LIGHT_GRAY           9
#define C_LIGHT_BLUE          10
#define C_LIGHT_GREEN         11
#define C_LIGHT_CYAN          12
#define C_LIGHT_RED           13
#define C_YELLOT_MAGENTA      14
#define C_WHITE               15


void set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    // Tell the VGA hardware which index number we want to modify
    __asm__ volatile("outb %0, %1" :: "a"(index), "Nd"(0x3C8));
    
    // Send the Red, Green, and Blue values sequentially to the data port
    __asm__ volatile("outb %0, %1" :: "a"(r), "Nd"(0x3C9));
    __asm__ volatile("outb %0, %1" :: "a"(g), "Nd"(0x3C9));
    __asm__ volatile("outb %0, %1" :: "a"(b), "Nd"(0x3C9));
}

void draw_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        VIDEO_MEMORY[y * SCREEN_WIDTH + x] = color;
    }
}

void draw_filled(int x, int y, int width, int height, uint8_t color) {
    for (int row = 0; row < height; row++) {
        // Calculate the exact starting memory address of this row
        uint8_t* row_start = &VIDEO_MEMORY[(y + row) * SCREEN_WIDTH + x];
        
        // Set the entire row width in a single, ultra-fast block operation
        memset(row_start, color, width); 
    }
}

void draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;  // Error total determining when to step across the grid

    while (x0 != x1 || y0 != y1) {
        draw_pixel(x0, y0, color);
        
        int e2 = 2 * err;
        
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_box(int x, int y, int size, uint8_t color) {
    draw_line(x, y, x + size, y, color);                  // Top
    draw_line(x, y + size, x + size, y + size, color);    // Bottom
    draw_line(x, y, x, y + size, color);                  // Left
    draw_line(x + size, y, x + size, y + size, color);    // Right
}

void draw_glyph(int x, int y, int width, int height, const uint8_t* glyph_data) {
    for (int row = 0; row < height; row++) {
        int current_y = y + row;
        if (current_y < 0 || current_y >= SCREEN_HEIGHT) 
            continue;
        
        for (int col = 0; col < width; col++) {
            int current_x = x + col;
            if (current_x < 0 || current_x >= SCREEN_WIDTH) 
                continue;
            
            uint8_t color = glyph_data[row * width + col];
            
            if (color != 0)
                VIDEO_MEMORY[current_y * SCREEN_WIDTH + current_x] = color;
        }
    }
}



void draw_sprite(int x, int y, int sprite_w, int sprite_h, int sheet_cols, 
                 int tile_x, int tile_y, const uint8_t* sheet_data) {
    int sheet_pixel_width = sheet_cols * sprite_w;
    int byte_offset = (tile_y * sprite_h * sheet_pixel_width) + (tile_x * sprite_w);
    
    const uint8_t* sub_glyph_ptr = sheet_data + byte_offset;
    
    for (int row = 0; row < sprite_h; row++) {
        int current_y = y + row;
        
        if (current_y < 0 || current_y >= 200) continue; 
        
        for (int col = 0; col < sprite_w; col++) {
            int current_x = x + col;
            
            if (current_x < 0 || current_x >= 320) continue;
            
            uint8_t color = sub_glyph_ptr[row * sheet_pixel_width + col];
            
            if (color == 0) 
                continue;
            
            VIDEO_MEMORY[current_y * 320 + current_x] = color;
        }
    }
}


const uint8_t smiley_glyph[] = {
    0,  0, 14, 14, 14, 14,  0,  0,
    0, 14, 14, 14, 14, 14, 14,  0,
   14, 14,  0, 14, 14,  0, 14, 14,
   14, 14,  0, 14, 14,  0, 14, 14,
   14, 14, 14, 14, 14, 14, 14, 14,
   14,  0, 14, 14, 14, 14,  0, 14,
    0, 14,  0,  0,  0,  0, 14,  0,
    0,  0, 14, 14, 14, 14,  0,  0
};

const uint8_t sprite_sheet[] = {
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  1,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  0,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  0,  0,
    0,  1,  0,  1,  0,
    0,  0,  1,  0,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  0,  1,  0,
    0,  0,  0,  1,  0,
    0,  0,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  1,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
    0,  0,  0,  0,  0,
    0,  0,  1,  0,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  1,  1,  0,
    0,  1,  0,  1,  0,
    0,  1,  0,  1,  0,
    0,  0,  0,  0,  0,
    
};



void kmain(uint32_t magic, multiboot_info_t* mbi) {
    draw_filled(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, C_BLACK);
    draw_filled(0, 0, SCREEN_WIDTH, 9, C_BLACK);
    
    draw_line(0, 10, SCREEN_WIDTH, 10, 0x19);
    
    draw_glyph(1, 1, 8, 8, smiley_glyph);
    
    //draw_box(0, 0, 100, 100);
    
    
    
    int screen_x = SCREEN_WIDTH / 2;
    int screen_y = SCREEN_HEIGHT / 2;
    int sprite_width  = 5;
    int sprite_height = 8;
    int columns_in_sheet = 1;
    
    int tile_column = 0; 
    int tile_row = 2;
    
    draw_sprite(screen_x, screen_y, sprite_width, sprite_height, 
                columns_in_sheet, tile_column, tile_row, sprite_sheet);
    
    
    /*
    // Center coordinates and a zoom factor
    // You can modify these to "move" and "zoom" around the fractal natively!
    double center_x = -0.743643887037158704752191506114774;
    double center_y =  0.131825904205311970493132056385139;
    double zoom = 0.005; // Smaller number = deeper zoom
    
    // Calculate the bounding box based on center and zoom aspect ratio
    double min_r = center_x - zoom;
    double max_r = center_x + zoom;
    double min_i = center_y - (zoom * ((double)SCREEN_HEIGHT / SCREEN_WIDTH));
    double max_i = center_y + (zoom * ((double)SCREEN_HEIGHT / SCREEN_WIDTH));
    
    double step_r = (max_r - min_r) / SCREEN_WIDTH;
    double step_i = (max_i - min_i) / SCREEN_HEIGHT;
    
    int max_iterations = 128; // Increased depth since floats can handle finer details
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        double ci = min_i + (y * step_i);
        
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            double cr = min_r + (x * step_r);
            
            double zr = 0.0;
            double zi = 0.0;
            int iteration = 0;
            
            while (iteration < max_iterations) {
                double zr2 = zr * zr;
                double zi2 = zi * zi;
                
                // Escape condition: Z^2 > 4
                if ((zr2 + zi2) > 4.0) {
                    break;
                }
                
                // Z_next = Z^2 + C
                zi = (2.0 * zr * zi) + ci;
                zr = zr2 - zi2 + cr;
                
                iteration++;
            }
            
            // Color mapping
            if (iteration == max_iterations) {
                VIDEO_MEMORY[y * SCREEN_WIDTH + x] = 0; // Black for inside the set
            } else {
                // Map the escape velocity to a cycling palette range
                VIDEO_MEMORY[y * SCREEN_WIDTH + x] = (uint8_t)((iteration % 32) + 32);
            }
        }
    }
    */
    
    
    while(1);
    
    char keyboard_string[255];
    char prompt_string[255];
    char virtual_key_map[255];
    
    // Display and keyboard for the console
    
    display_init();
    display_clear();
    
    display_cursor_set_line(0);
    display_cursor_set_position(0);
    
    kb_init();
    kb_map_init(virtual_key_map, sizeof(virtual_key_map));
    
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    // Heap allocator
    
    uint32_t heap_sz  = ((uint32_t)1024 * (uint32_t)1024 * (uint32_t)1024 * 4) - 1;
    uint32_t block_sz = 64;
    
    uint32_t heap_start = ((uint32_t)_kernel_end + 4095) & ~4095;
    heap_set_base_address(heap_start);
    
    heap_init(block_sz, heap_sz);
    
    kernel_init();
    
    console_prompt_set_string("/>");
    print("kernel v0.0.0\n");
    
    console_prompt_print();
    while(1) {
        
        kb_event_handler();
    }
    
}
