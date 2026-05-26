#ifndef WINDOW_HANDLE_H
#define WINDOW_HANDLE_H

#include <stdint.h>

struct WindowHandle {
    struct WindowHandle* next;
    
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    
    uint16_t surface_x;
    uint16_t surface_y;
    uint16_t surface_w;
    uint16_t surface_h;
    
    uint8_t border_width;
    uint8_t titlebar_height;
    
    uint32_t border_color;
    uint32_t background_color;
    
    uint32_t title_color_low;
    uint32_t title_color_high;
    
    uint32_t inactive_color_low;
    uint32_t inactive_color_high;
    
    uint8_t flags;
    
    void (*event_callback)(struct WindowHandle* window);
};

#endif
