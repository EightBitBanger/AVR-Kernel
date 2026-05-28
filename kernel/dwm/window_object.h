#ifndef WINDOW_OBJECT_H
#define WINDOW_OBJECT_H

#include <stdint.h>
#include <kernel/dwm/window_handle.h>
#include <kernel/dwm/window_events.h>

struct WindowObject {
    WindowHandle id;
    
    uint8_t flags;
    
    void(*event_callback)(WindowHandle, wEvent);
    
    // Position and size
    
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    
    // Drawing area
    
    uint16_t surface_x;
    uint16_t surface_y;
    uint16_t surface_w;
    uint16_t surface_h;
    
    // Layout
    
    uint8_t border_width;
    uint8_t titlebar_height;
    
    // Colors
    
    uint32_t border_color;
    uint32_t background_color;
    
    uint32_t title_color_low;
    uint32_t title_color_high;
    
    uint32_t inactive_color_low;
    uint32_t inactive_color_high;
};

#endif
