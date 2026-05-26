#ifndef DWM_WINDOW_CONTEXT_H
#define DWM_WINDOW_CONTEXT_H

struct WindowContext {
    Point point;
    bool left_button_pressed;
    bool right_button_pressed;
    bool window_moved;
    
    int old_win_min_x;
    int old_win_min_y;
    int old_win_max_x;
    int old_win_max_y;
    
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    
    uint32_t cursor_width;
    uint32_t cursor_height;
};

#endif
