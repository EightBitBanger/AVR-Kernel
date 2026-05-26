#include <kernel/dwm/icons.h>
#include <kernel/dwm/rendering/sprite.h>

const struct Sprite rc_cursor_pointer = {
    .width  = 11,
    .height = 11,
    .palette_size = 8,
    
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFF909090),
        COLOR(0xFF888888),
        COLOR(0xFF747474),
        COLOR(0xFF606060),
        COLOR(0xFF585858),
        COLOR(0xFF444444),
        COLOR(0xFFFFFFFF),
        
        7,7,7,7,7,7,7,7,7,0,0,
        7,5,4,3,2,1,1,1,1,0,0,
        7,6,4,3,2,2,1,7,0,0,0,
        7,6,4,3,3,2,7,0,0,0,0,
        7,6,4,3,3,2,7,0,0,0,0,
        7,6,4,4,3,3,2,7,0,0,0,
        7,5,5,7,7,3,2,1,7,0,0,
        7,5,7,0,0,7,3,2,1,7,0,
        7,5,0,0,0,0,7,3,2,1,7,
        0,0,0,0,0,0,0,7,4,1,7,
        0,0,0,0,0,0,0,0,7,7,7
    }
};

const struct Sprite rc_button_check = {
    .width  = 8,
    .height = 8,
    .palette_size = 2,
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFF00E000),
        
        0,0,0,0,0,0,0,1,
        0,0,0,0,0,0,1,1,
        0,0,0,0,0,1,1,0,
        1,0,0,0,1,1,0,0,
        1,1,0,1,1,0,0,0,
        0,1,1,1,0,0,0,0,
        0,0,1,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    }
};

const struct Sprite rc_button_x = {
    .width  = 8,
    .height = 8,
    .palette_size = 2,
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFFE00A0A),
        
        1,1,0,0,0,0,1,1,
        0,1,1,0,0,1,1,0,
        0,0,1,1,1,1,0,0,
        0,0,0,1,1,0,0,0,
        0,0,0,1,1,0,0,0,
        0,0,1,1,1,1,0,0,
        0,1,1,0,0,1,1,0,
        1,1,0,0,0,0,1,1
    }
};

const struct Sprite rc_button_plus = {
    .width  = 8,
    .height = 8,
    .palette_size = 2,
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFF00A2E8),
        
        0,0,0,1,1,0,0,0,
        0,0,0,1,1,0,0,0,
        0,0,0,1,1,0,0,0,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        0,0,0,1,1,0,0,0,
        0,0,0,1,1,0,0,0,
        0,0,0,1,1,0,0,0
    }
};

const struct Sprite rc_icon_arrow_down = {
    .width  = 8,
    .height = 8,
    .palette_size = 2,
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFFE0E0E0),
        
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,
        0,1,1,1,1,1,1,0,
        0,0,1,1,1,1,0,0,
        0,0,0,1,1,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    }
};

const struct Sprite rc_icon_arrow_up = {
    .width  = 8,
    .height = 8,
    .palette_size = 2,
    .data = {
        COLOR(0xFF000000),
        COLOR(0xFFE0E0E0),
        
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,1,1,0,0,0,
        0,0,1,1,1,1,0,0,
        0,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    }
};
