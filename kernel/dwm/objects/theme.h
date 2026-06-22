#ifndef DWM_THEME_H
#define DWM_THEME_H


struct UITheme {
    
    // General
    
    uint32_t bg_color = 0xFF0E0E1A;
    
    // Context menu
    
    uint32_t context_menus_bg         = 0x8F222222;
    uint32_t context_menus_border     = 0x8F444444;
    uint32_t context_menus_separator  = 0x8F111111;
    uint32_t context_menus_highlight  = 0x8F777777;
    uint32_t context_menus_text       = 0x8FE0E0E0;
    
    // Window
    
    uint32_t window_border;
    uint32_t window_background;
    uint32_t window_title_text;
    
    uint32_t window_title_low;
    uint32_t window_title_high;
    
    uint32_t window_inactive_low;
    uint32_t window_inactive_high;
    
};

#endif
