#ifndef WINDOW_BUTTON_H
#define WINDOW_BUTTON_H

#include <stdint.h>

struct WindowButton {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    
    uint16_t event;
    struct Image img;
};

#endif
