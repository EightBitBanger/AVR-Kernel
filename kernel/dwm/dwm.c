#include <kernel/dwm/dwm.h>
#include <kernel/util/string.h>
#include <kernel/arch/x86/heap.h>

#include <kernel/arch/x86/drivers/draw.h>
#include <kernel/console/mouse.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>

extern const uint8_t char_rom[];

uint32_t bg_color = 0xFF0E0E1A;

// Images

struct Image cursor;
struct Image button_x;

// Window linked list

struct list_node* window_head = NULL;
struct list_node* window_tail = NULL;

struct list_node* icon_head = NULL;
struct list_node* icon_tail = NULL;

struct WindowContext window_context;

// Dragging

struct WindowObject* dragged_window = NULL;
int drag_offset_x = 0;
int drag_offset_y = 0;
bool old_left_button_pressed = false;
bool old_right_button_pressed = false;

struct WindowObject* event_window = NULL;

// Window creation ID counter
uint32_t next_window_id;

// Icon dragging state

struct IconObject* dragged_icon = NULL;
int icon_drag_offset_x = 0;
int icon_drag_offset_y = 0;

// Mouse tracking and click state

struct IconObject* last_clicked_icon = NULL;
uint32_t last_icon_click_time = 0;

Point mouse_old;

// Context menu
struct ContextMenu context_menu;

// Taskbar

uint16_t taskbar_height = 28;


void dwm_initiate(void) {
    window_context.cursor_width   = 0;
    window_context.cursor_height  = 0;
    window_context.mouse = mouse_get_position();
    
    window_context.window_moved = false;
    window_context.icon_moved = false;
    window_context.menu_moved = false;
    
    window_context.old_win_min_x = 0;
    window_context.old_win_min_y = 0;
    window_context.old_win_max_x = 0;
    window_context.old_win_max_y = 0;
    
    window_context.old_icon_min_x = 0;
    window_context.old_icon_min_y = 0;
    window_context.old_icon_max_x = 0;
    window_context.old_icon_max_y = 0;
    
    window_context.old_menu_min_x = 0;
    window_context.old_menu_min_y = 0;
    window_context.old_menu_max_x = 0;
    window_context.old_menu_max_y = 0;
    
    context_menu.visible = false;
    context_menu.x = 0;
    context_menu.y = 0;
    context_menu.w = 0;
    context_menu.h = 0;
    
    context_menu.color_bg         = 0x8F222222;
    context_menu.color_border     = 0x8F444444;
    context_menu.color_separator  = 0x8F111111;
    context_menu.color_highlight  = 0x8F777777;
    context_menu.color_text       = 0x8FE0E0E0;
    
    context_menu.hovered_item = -1;
    
    context_menu.item_height = 0;
    context_menu.item_count = 0;
    
    
    next_window_id = 0;
    
    window_head = NULL;
    window_tail = NULL;
    
    // Center mouse
    
    Point display_center;
    display_center.x = display_get_width() / 2;
    display_center.y = display_get_height() / 2;
    
    mouse_set_position(display_center.x, display_center.y);
    mouse_old = (Point){display_get_width(), display_get_height()};
    
    // Load theme
    
    button_x.data = malloc(sizeof(uint32_t) * rc_button_x.width * rc_button_x.height);
    sprite_get_bitmap(button_x.data, &rc_button_x);
    button_x.width = rc_button_x.width;
    button_x.height = rc_button_x.height;
    
    // Blank the screen in preparation for window management
    
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), bg_color);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
}

WindowHandle create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void(*event_callback)(WindowHandle, wEvent)) {
    WindowHandle handle = 0;
    
    struct WindowObject* window_object = malloc(sizeof(struct WindowObject));
    if (window_object == NULL) 
        return handle;
    
    memset(window_object, 0x00, sizeof(struct WindowObject));
    
    // Allocate frame buffer
    uint32_t frame_buffer_sz = width * height * sizeof(uint32_t);
    window_object->frame_buffer = (uint32_t*)malloc( frame_buffer_sz );
    
    if (window_object->frame_buffer == NULL) {
        free(window_object);
        return handle;
    }
    
    memset(window_object->frame_buffer, 0x11, frame_buffer_sz);
    
    uint32_t candidate_id = next_window_id++;
    if (candidate_id == 0) candidate_id = next_window_id++;
    
    bool id_check = true;
    while (id_check) {
        id_check = false;
        for (struct list_node* node = window_head; node != NULL; node = node->next) {
            struct WindowObject* window = (struct WindowObject*)node->data;
            if (window->id == candidate_id) {
                id_check = true;
                candidate_id = next_window_id++;
                if (candidate_id == 0) candidate_id = next_window_id++;
                break;
            }
        }
    }
    
    window_object->id = candidate_id;
    window_object->border_width = 1;
    window_object->titlebar_height = 28;
    window_object->x = x;
    window_object->y = (y < 0) ? 0 : y;
    window_object->w = width;
    window_object->h = height + window_object->titlebar_height;
    
    window_object->surface_x = window_object->x;
    window_object->surface_y = window_object->y + window_object->titlebar_height + 1;
    window_object->surface_w = window_object->w;
    window_object->surface_h = window_object->h - window_object->titlebar_height;
    
    window_object->buffer_w = width;
    window_object->buffer_h = height;
    
    window_object->border_color         = 0xFF2A2A2A;
    window_object->background_color     = 0xFF6F6F6F;
    window_object->title_color_low      = 0xFF008000;
    window_object->title_color_high     = 0xFF001900;
    window_object->inactive_color_low   = 0xFF505050;
    window_object->inactive_color_high  = 0xFF101010;
    window_object->flags = WINDOW_FLAG_REDRAW | WINDOW_FLAG_REFRESH;
    window_object->event_callback = event_callback;
    
    if (!list_append(&window_head, &window_tail, window_object)) {
        free(window_object);
        return 0;
    }
    
    handle = window_object->id;
    return handle;
}

void destroy_window(WindowHandle handle) {
    if (handle == 0) return;
    
    struct WindowObject* window_handle = NULL;
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* win = (struct WindowObject*)node->data;
        if (win->id == handle) {
            window_handle = win;
            break;
        }
    }
    
    if (window_handle == NULL) return; 
    
    if (dragged_window == window_handle) 
        dragged_window = NULL;
    
    int destroyed_min_x = window_handle->x - window_handle->border_width;
    int destroyed_max_x = window_handle->x + window_handle->w + window_handle->border_width;
    int destroyed_min_y = window_handle->y - window_handle->border_width;
    int destroyed_max_y = window_handle->y + window_handle->h + window_handle->border_width;
    
    window_context.old_win_min_x = destroyed_min_x;
    window_context.old_win_min_y = destroyed_min_y;
    window_context.old_win_max_x = destroyed_max_x;
    window_context.old_win_max_y = destroyed_max_y;
    window_context.window_moved = true; 
    
    list_remove(&window_head, &window_tail, window_handle);
    
    if (window_tail != NULL) {
        struct WindowObject* tail_win = (struct WindowObject*)window_tail->data;
        tail_win->flags |= WINDOW_FLAG_REDRAW;
    }
    
    if (window_handle->frame_buffer != NULL) {
        free(window_handle->frame_buffer);
    }
    
    free(window_handle);
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* win = (struct WindowObject*)node->data;
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

struct IconObject* create_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t* sprite) {
    struct IconObject* icon = malloc(sizeof(struct IconObject));
    if (icon == NULL) return NULL;
    
    memset(icon, 0x00, sizeof(struct IconObject));
    
    icon->x = x;
    icon->y = y;
    icon->width = width;
    icon->height = height;
    icon->icon_sprite = sprite;
    icon->flags = 0; 
    
    icon->bounds_x = -1;
    icon->bounds_y = -1;
    icon->bounds_w = width + 2;
    icon->bounds_h = height + 2;
    
    if (!list_append(&icon_head, &icon_tail, icon)) {
        free(icon);
        return NULL;
    }
    
    dwm_draw_redraw(icon->x, icon->y, icon->width, icon->height);
    return icon;
}

void destroy_icon(struct IconObject* icon) {
    if (icon == NULL) return;
    
    size_t length = strlen(icon->name);
    uint16_t string_width = length * 6;
    
    int16_t min_x = icon->x + ((icon->width - string_width) / 2);
    int16_t max_x = min_x + string_width;
    
    // Prevent redraw below the icon asset size
    if (min_x > icon->x) min_x = icon->x;
    if (max_x < icon->x + icon->width) max_x = icon->x + icon->width;
    
    window_context.old_icon_min_x = min_x;
    window_context.old_icon_min_y = icon->y;
    window_context.old_icon_max_x = max_x;
    window_context.old_icon_max_y = icon->y + icon->height + 40;
    window_context.icon_moved = true;
    
    bool explicitly_found = false;
    for (struct list_node* node = icon_head; node != NULL; node = node->next) {
        if (node->data == icon) {
            explicitly_found = true;
            break;
        }
    }
    if (!explicitly_found) return;
    
    dwm_draw_redraw(icon->x, icon->y, icon->width, icon->height);
    list_remove(&icon_head, &icon_tail, icon);
    free(icon);
}

struct WindowObject* dwm_get_window_by_id(uint32_t id) {
    if (id == 0) return NULL;
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        if (window->id == id) 
            return window;
    }
    return NULL;
}

void dwm_set_focus(struct WindowObject* target) {
    if (target == NULL || (window_tail != NULL && window_tail->data == target)) {
        return; 
    }
    
    if (list_remove(&window_head, &window_tail, target)) {
        list_append(&window_head, &window_tail, target);
    }
}

void dwm_window_set_focus(WindowHandle handle) {
    struct WindowObject* target = dwm_get_window_by_id(handle);
    if (target != NULL) {
        dwm_set_focus(target);
    }
}

void dwm_draw_text(int16_t x, int16_t y, const char* text, uint32_t color) {
    if (event_window == NULL) return;
    size_t length = strlen(text);
    for (unsigned int i=0; i < length; i++) 
        draw_glyph(char_rom, text[i], x + (i * 6), y, color, 0xFF000000, 0xFF000000);
}

void dwm_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color, bool filled) {
    if (event_window == NULL) return;
    if (filled) 
        draw_rect_filled(x, y, w, h, color);
    else 
        draw_rect(x, y, w, h, color);
}

void dwm_set_cursor(uint32_t* sprite, int16_t width, int16_t height) {
    cursor.data = sprite;
    cursor.width = width;
    cursor.height = height;
}

void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t target_min_x = x;
    int16_t target_max_x = x + w;
    int16_t target_min_y = y;
    int16_t target_max_y = y + h;
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        int win_min_x = window->x - window->border_width;
        int win_max_x = window->x + window->w + window->border_width;
        int win_min_y = window->y - window->border_width;
        int win_max_y = window->y + window->h + window->border_width;
        
        bool separate_x = (win_min_x >= target_max_x) || (win_max_x <= target_min_x);
        bool separate_y = (win_min_y >= target_max_y) || (win_max_y <= target_min_y);
        bool overlaps   = !(separate_x || separate_y);
        
        if (overlaps) 
            window->flags |= WINDOW_FLAG_REFRESH;
    }
    
    window_context.old_win_min_x = target_min_x;
    window_context.old_win_min_y = target_min_y;
    window_context.old_win_max_x = target_max_x;
    window_context.old_win_max_y = target_max_y;
    window_context.window_moved  = true; 
}

void dwm_invalidate_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t max_x_target = x + w;
    int16_t max_y_target = y + h;

    if (window_context.window_moved) {
        if (x < window_context.min_x) window_context.min_x = x;
        if (y < window_context.min_y) window_context.min_y = y;
        if (max_x_target > window_context.max_x) window_context.max_x = max_x_target;
        if (max_y_target > window_context.max_y) window_context.max_y = max_y_target;
    } else {
        window_context.min_x = x;
        window_context.min_y = y;
        window_context.max_x = max_x_target;
        window_context.max_y = max_y_target;
        window_context.window_moved = true;
    }
}

void dwm_calculate_icon_bounds(struct IconObject* icon) {
    size_t length = strlen(icon->name);
    int16_t text_width = length * 6;
    int16_t text_height = 8;        // Assuming standard 8px tall font glyphs
    int16_t icon_text_height = 45;  // Matches your current_icon->y + 35 offset
    
    if (text_width > icon->width) {
        // Text is wider than the sprite: bounds start to the left of icon->x
        icon->bounds_x = (icon->width - text_width) / 2;
        icon->bounds_w = text_width;
    } else {
        // Sprite is wider than or equal to text: bounds match the sprite
        icon->bounds_x = 0;
        icon->bounds_w = icon->width;
    }
    
    icon->bounds_y = 0;
    icon->bounds_h = icon_text_height + text_height;
}

void dwm_upload_window_buffer_to_backbuffer(struct WindowObject* window, uint32_t* frame_buffer, uint32_t screen_stride,
                                            int clip_x, int clip_y, int clip_w, int clip_h) {
    int client_start_x = window->x;
    int client_start_y = window->y + window->titlebar_height;
    
    int local_start_x = clip_x - client_start_x;
    int local_start_y = clip_y - client_start_y;
    
    if (local_start_x < 0) { clip_w += local_start_x; local_start_x = 0; }
    if (local_start_y < 0) { clip_h += local_start_y; local_start_y = 0; }
    
    if (local_start_x + clip_w > (int)window->buffer_w) clip_w = (int)window->buffer_w - local_start_x;
    if (local_start_y + clip_h > (int)window->buffer_h) clip_h = (int)window->buffer_h - local_start_y;
    
    if (clip_w <= 0 || clip_h <= 0) return;
    
    for (int i = 0; i < clip_h; i++) {
        int current_local_y  = local_start_y + i;
        int current_screen_y = clip_y + i;
        
        // Offset src by the local X position, and dest by the screen clip_x position
        uint32_t* src  = &window->frame_buffer[(current_local_y * window->buffer_w) + local_start_x];
        uint32_t* dest = &frame_buffer[(current_screen_y * screen_stride) + clip_x];
        int count = clip_w;
        
        asm volatile (
            "cld;\n\t"
            "rep movsd;\n\t"
            : "+S"(src), "+D"(dest), "+c"(count)
            :
            : "memory"
        );
    }
}
