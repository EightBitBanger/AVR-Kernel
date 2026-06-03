#ifndef ICON_OBJECT_H
#define ICON_OBJECT_H

#include <stdint.h>

struct IconObject {
    uint8_t flags;
    
    char name[16];
    
    // Position and size
    
    uint16_t x;
    uint16_t y;
    
    // Image
    
    uint16_t width;
    uint16_t height;
    
    // Bounds
    
    int16_t bounds_x;
    int16_t bounds_y;
    int16_t bounds_w;
    int16_t bounds_h;
    
    uint32_t* icon_sprite;
};

#endif
