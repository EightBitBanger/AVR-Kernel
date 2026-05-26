#include <kernel/dwm/dwm.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/heap.h>

uint32_t bg_color = 0x000E0E1A;

uint32_t* cursor_sprite;
uint32_t* button_x;

Point old_point;

struct WindowHandle* window_head = NULL;
struct WindowHandle* window_tail = NULL;

struct WindowHandle* dragged_window = NULL;
int drag_offset_x = 0;
int drag_offset_y = 0;

bool old_left_button_pressed = false;
bool old_right_button_pressed = false;

struct WindowContext windowContext;


void dwm_initiate(void) {
    windowContext.cursor_width   = 11;
    windowContext.cursor_height  = 11;
    windowContext.point = mouse_get_position();
    windowContext.window_moved = false;
    windowContext.old_win_min_x = 0;
    windowContext.old_win_min_y = 0;
    windowContext.old_win_max_x = 0;
    windowContext.old_win_max_y = 0;
    
    window_head = NULL;
    window_tail = NULL;
    
    Point display_center;
    display_center.x = display_get_width() / 2;
    display_center.y = display_get_height() / 2;
    
    mouse_set_position(display_center.x, display_center.y);
    
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), bg_color);
    
    old_point = (Point){display_get_width(), display_get_height()};
    
    cursor_sprite = malloc(sizeof(uint32_t) * rc_cursor_pointer.width * rc_cursor_pointer.height);
    button_x = malloc(sizeof(uint32_t) * rc_button_x.width * rc_button_x.height);
    
    sprite_create_bitmap(cursor_sprite, &rc_cursor_pointer);
    sprite_create_bitmap(button_x, &rc_button_x);
    
    draw_flush_region(0, 0, display_get_width(), display_get_height());
}

struct WindowHandle* create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    struct WindowHandle* window_handle = malloc(sizeof(struct WindowHandle));
    if (window_handle == NULL) 
        return (struct WindowHandle*)(uintptr_t*)KMALLOC_NULL;
    
    memset(window_handle, 0x00, sizeof(struct WindowHandle));
    
    window_handle->w = width;
    window_handle->h = height;
    window_handle->border_width = 1;
    window_handle->titlebar_height = 20;
    
    // Clamping y directly to 0 ensures the title bar won't clip out of sight.
    window_handle->y = (y < 0) ? 0 : y; 
    window_handle->x = x; 
    
    // Calculate initial user draw space bounds
    window_handle->surface_x = window_handle->x;
    window_handle->surface_y = window_handle->y + window_handle->titlebar_height + 1;
    window_handle->surface_w = window_handle->w;
    window_handle->surface_h = window_handle->h - window_handle->titlebar_height;
    
    window_handle->border_color         = 0x002A2A2A;
    window_handle->background_color     = 0x006F6F6F;
    
    window_handle->title_color_low      = 0x00008000;
    window_handle->title_color_high     = 0x00001000;
    window_handle->inactive_color_low   = 0x00505050;
    window_handle->inactive_color_high  = 0x00101010;
    
    window_handle->flags = WINDOW_FLAG_REDRAW;
    window_handle->next = NULL;
    
    window_handle->event_callback = NULL;
    
    if (window_head == NULL) {
        window_head = window_handle;
        window_tail = window_handle;
    } else {
        window_tail->next = window_handle;
        window_tail = window_handle;
    }
    
    return window_handle;
}

void destroy_window(struct WindowHandle* window_handle) {
    if (window_handle == NULL) return;
    
    if (dragged_window == window_handle) 
        dragged_window = NULL;
    
    int destroyed_min_x = window_handle->x - window_handle->border_width;
    int destroyed_max_x = window_handle->x + window_handle->w + window_handle->border_width;
    int destroyed_min_y = window_handle->y - window_handle->border_width;
    int destroyed_max_y = window_handle->y + window_handle->h + window_handle->border_width;
    
    windowContext.old_win_min_x = destroyed_min_x;
    windowContext.old_win_min_y = destroyed_min_y;
    windowContext.old_win_max_x = destroyed_max_x;
    windowContext.old_win_max_y = destroyed_max_y;
    windowContext.window_moved = true; 
    
    struct WindowHandle* prev = NULL;
    struct WindowHandle* curr = window_head;
    
    while (curr != NULL && curr != window_handle) {
        prev = curr;
        curr = curr->next;
    }
    if (curr == NULL) return; 
    
    if (prev != NULL) {
        prev->next = window_handle->next;
    } else {
        window_head = window_handle->next;
    }
    
    if (window_handle == window_tail) 
        window_tail = prev; 
    
    if (window_tail != NULL) 
        window_tail->flags |= WINDOW_FLAG_REDRAW;
    
    free(window_handle);
    
    for (struct WindowHandle* win = window_head; win != NULL; win = win->next) {
        int win_min_x = win->x - win->border_width;
        int win_max_x = win->x + win->w + win->border_width;
        int win_min_y = win->y - win->border_width;
        int win_max_y = win->y + win->h + win->border_width;
        
        bool separate_x = (win_min_x >= destroyed_max_x) || (win_max_x <= destroyed_min_x);
        bool separate_y = (win_min_y >= destroyed_max_y) || (win_max_y <= destroyed_min_y);
        bool overlaps   = !(separate_x || separate_y);
        
        if (overlaps) {
            win->flags |= WINDOW_FLAG_REDRAW;
        }
    }
}

void dwm_set_focus(struct WindowHandle* target) {
    if (target == NULL || target == window_tail) {
        return; 
    }
    
    struct WindowHandle* prev = NULL;
    struct WindowHandle* curr = window_head;
    while (curr != NULL && curr != target) {
        prev = curr;
        curr = curr->next;
    }
    
    if (curr == NULL) return; 
    
    if (prev != NULL) {
        prev->next = target->next;
    } else {
        window_head = target->next;
    }
    
    target->next = NULL;
    if (window_tail != NULL) {
        window_tail->next = target;
    } else {
        window_head = target; 
    }
    window_tail = target;
}
