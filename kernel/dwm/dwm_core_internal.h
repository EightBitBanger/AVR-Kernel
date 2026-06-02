#ifndef DWM_INTERNAL_CORE_CONPONENTS_H
#define DWM_INTERNAL_CORE_CONPONENTS_H

extern uint32_t bg_color;

// Drag movement
extern struct WindowObject* dragged_window;
extern int drag_offset_x;
extern int drag_offset_y;

extern struct IconObject* dragged_icon;
extern int icon_drag_offset_x;
extern int icon_drag_offset_y;

// Object lists
extern struct list_node* window_head;
extern struct list_node* window_tail;

extern struct list_node* icon_head;
extern struct list_node* icon_tail;

extern struct WindowContext window_context;
extern Point mouse_old;

extern struct Image cursor;
extern struct Image button_close1;
extern struct Image wallpaper_image;

extern bool old_left_button_pressed;
extern bool old_right_button_pressed;

extern struct WindowObject* event_window;

// Double click timing
extern struct IconObject* last_clicked_icon;
extern uint32_t last_icon_click_time;

// Context menu
extern struct ContextMenu context_menu;

// Taskbar
extern WindowHandle w_taskbar;
extern uint16_t taskbar_height;

// Images

extern struct Image cursor;
extern struct Image button_close;


extern const uint8_t char_rom[];


// Built in event handlers

void callback_button_close_handler(WindowHandle handle, wEvent event);

// Internal routines

struct WindowObject* dwm_window_create(WindowClass w_class, uint16_t w_style, WindowProcedure proc);

void dwm_draw_desktop(const struct WindowContext* ctx);
void dwm_draw_window(struct WindowObject* window_handle);
void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h);

void dwm_update_mouse(struct WindowContext* ctx);
void dwm_update_window_dragging(struct WindowContext* ctx);
void dwm_update_icon_dragging(struct WindowContext* ctx);

void dwm_calculate_icon_bounds(struct IconObject* icon);
void dwm_invalidate_region(int16_t x, int16_t y, int16_t w, int16_t h);
void dwm_get_absolute_position(struct WindowObject* window, int* out_x, int* out_y);
void dwm_cascade_child_positions(struct WindowObject* parent);
void dwm_process_events(struct WindowObject* window);

void dwm_set_focus(struct WindowObject* target);
void dwm_calculate_flush_region(struct WindowContext* ctx);

void dwm_upload_window_buffer_to_backbuffer(struct WindowObject* window, uint32_t* frame_buffer, uint32_t screen_stride, 
                                            int clip_x, int clip_y, int clip_w, int clip_h);

#endif
