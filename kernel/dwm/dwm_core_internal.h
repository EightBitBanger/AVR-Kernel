#ifndef DWM_INTERNAL_CORE_CONPONENTS_H
#define DWM_INTERNAL_CORE_CONPONENTS_H

#include <kernel/dwm/objects/window_button.h>

#define CONTEXT_MENU_DESKTOP  0x01
#define CONTEXT_MENU_ICON     0x02

extern uint32_t bg_color;

// Drag movement
extern struct WindowObject* dragged_window;
extern int drag_offset_x;
extern int drag_offset_y;

extern struct IconObject* dragged_icon;
extern int icon_drag_offset_x;
extern int icon_drag_offset_y;

extern struct WindowObject* resizing_window;
extern int resize_offset_x;
extern int resize_offset_y;

// Object lists
extern struct list_node* window_head;
extern struct list_node* window_tail;

extern struct list_node* icon_head;
extern struct list_node* icon_tail;

extern struct WindowContext window_context;
extern Point mouse_old;

extern bool old_left_button_pressed;
extern bool old_right_button_pressed;

extern struct WindowObject* event_window;

// Double click timing
extern struct IconObject* last_clicked_icon;
extern uint32_t last_icon_click_time;

// Context menu
extern struct ContextMenu context_menus[];
extern uint8_t context_menu_count;
extern uint16_t context_menu_directive;

// Taskbar
extern WindowHandle w_taskbar;
extern uint16_t taskbar_height;

// Images

extern struct Image current_cursor;

extern const uint8_t char_rom[];

// Built in event handlers

void callback_button_close_handler(WindowHandle handle, wEvent event);
void callback_taskbar_handler(WindowHandle handle, wEvent event);

// Internal routines

struct WindowObject* dwm_window_create(WindowClass w_class, uint16_t w_style, WindowProcedure proc);

void dwm_resource_load(const char* name, void* resource);
void* dwm_resource_find(const char* name);
void dwm_resource_unload(const char* name);

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
void dwm_process_context_menu_events(uint16_t index);

void dwm_set_focus(struct WindowObject* target);
void dwm_calculate_flush_region(struct WindowContext* ctx);

void dwm_upload_window_buffer_to_backbuffer(struct WindowObject* window, uint32_t* frame_buffer, uint32_t screen_stride, 
                                            int clip_x, int clip_y, int clip_w, int clip_h);

void window_add_button(struct WindowObject* window, int16_t x, int16_t y, uint16_t width, uint16_t height, 
                       uint16_t event, struct Image sprite);

#endif
