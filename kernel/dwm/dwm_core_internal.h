#ifndef DWM_INTERNAL_CORE_CONPONENTS_H
#define DWM_INTERNAL_CORE_CONPONENTS_H

#include <kernel/dwm/objects/window_button.h>
#include <kernel/dwm/dwm_context_menu.h>

#include <kernel/dwm/dwm_platform.h>
#include <kernel/dwm/flags.h>

#define MAX_CONTEXT_MENUS 8

struct DWMWorkspace {
    struct list_node* window_head;
    struct list_node* window_tail;
    
    struct list_node* icon_head;
    struct list_node* icon_tail;
    
    struct map_node* resource_head;
    struct map_node* resource_tail;
    
    uint32_t next_window_id;
    uint32_t next_edit_field_id;
};

struct DWMTaskbar {
    WindowHandle window;
    
    uint16_t height;
};

struct DWMTheme {
    uint32_t bg_color;
    
    // Window frame
    
    uint32_t w_border;
    uint32_t w_background;
    uint32_t w_title_text;
    
    uint32_t w_title_low;
    uint32_t w_title_high;
    
    uint32_t w_inactive_low;
    uint32_t w_inactive_high;
    
    // Context menu
    
    uint32_t ctx_bg;
    uint32_t ctx_border;
    uint32_t ctx_separator;
    uint32_t ctx_highlight;
    uint32_t ctx_text;
    
};

struct DWMDragDrop {
    struct WindowObject* dragged_window;
    int drag_offset_x;
    int drag_offset_y;
    
    struct IconObject* dragged_icon;
    int icon_drag_offset_x;
    int icon_drag_offset_y;
    
    struct WindowObject* dragged_resizing;
    int resize_offset_x;
    int resize_offset_y;
};

struct DWMContext {
    struct WindowContext window_context;
    struct WindowObject* event_window;
    
    struct IconObject* focused_icon;
    struct IconObject* last_focused_icon;
    
    uint32_t last_icon_click_time;  // Double click timing
};

struct DWMInput {
    uint16_t last_key_pressed;
    
    Point mouse_last;
    
    bool last_left_button_pressed;
    bool last_right_button_pressed;
};

struct DWMContextMenu {
    struct ContextMenu menus[MAX_CONTEXT_MENUS];
    
    uint8_t menu_count;
    uint16_t menu_directive;
    struct WindowObject* handle;
};

struct DWMImages {
    struct Image current_cursor;
    
};

struct DWMCascade {
    uint16_t x;
    uint16_t y;
    
    uint16_t h;
    uint16_t w;
    
    uint16_t max;
};

extern struct DWMTheme theme;

extern struct DWMWorkspace    workspace;
extern struct DWMContext      context;
extern struct DWMTaskbar      taskbar;
extern struct DWMDragDrop     dragdrop;
extern struct DWMInput        input;
extern struct DWMContextMenu  ctxmenu;
extern struct DWMImages       images;
extern struct DWMCascade      cascade;


// UI

bool dwm_create_context_menu(int x, int y, uint32_t directive, const char* items[], int item_count);

void window_add_button(struct WindowObject* window, int16_t x, int16_t y, uint16_t width, uint16_t height, 
                       uint16_t event, struct Image* sprite);

// Built in event handlers

void callback_properties_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);
void callback_message_box_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);
void callback_deletion_dialog_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);
void callback_taskbar_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);

// Internal routines

struct WindowObject* dwm_allocate_window(WindowClass w_class, uint16_t w_style, WindowProcedure proc);

void dwm_resource_load(const char* name, void* resource);
void* dwm_resource_find(const char* name);
void dwm_resource_unload(const char* name);
void dwm_resource_sprite_load(const char* resource_name, const struct Sprite* sprite);

void dwm_draw_desktop(const struct WindowContext* ctx);
void dwm_draw_window(struct WindowObject* window_handle);
void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h);

void dwm_update_mouse(struct WindowContext* ctx);
void dwm_update_window_dragging(struct WindowContext* ctx);
void dwm_update_icon_dragging(struct WindowContext* ctx);
void dwm_update_window_resizing(struct WindowContext* ctx);

void dwm_sync_child_positions(struct WindowObject* parent);
void dwm_calculate_icon_bounds(struct IconObject* icon);
void dwm_invalidate_region(int16_t x, int16_t y, int16_t w, int16_t h);
void dwm_get_absolute_position(struct WindowObject* window, int* out_x, int* out_y);
void dwm_cascade_child_positions(struct WindowObject* parent);
void dwm_process_window_events(struct WindowObject* window);
void dwm_process_context_menu_events(struct WindowContext* ctx, uint16_t index);

void dwm_set_focus(struct WindowObject* target);
void dwm_calculate_flush_region(struct WindowContext* ctx);

struct WindowObject* dwm_get_window_by_id(uint32_t id);

struct WindowObject* dwm_get_root_parent(struct WindowObject* window);

void dwm_upload_window_buffer_to_backbuffer(struct WindowObject* window, uint32_t* frame_buffer, uint32_t screen_stride, 
                                            int clip_x, int clip_y, int clip_w, int clip_h);

#endif
