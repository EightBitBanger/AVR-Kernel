#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>

#include <kernel/console/display.h>
#include <kernel/dwm/icon_object.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>

#define DOUBLE_CLICK_THRESHOLD_MS   800

extern uint32_t bg_color;

extern struct WindowObject* dragged_window;
extern int drag_offset_x;
extern int drag_offset_y;

// Global heads/tails updated to use external generic list nodes
extern struct list_node* window_head;
extern struct list_node* window_tail;

extern struct window_context window_context;
extern Point old_point;

extern uint16_t cursor_width;
extern uint16_t cursor_height;

extern uint32_t* cursor_sprite;
extern uint32_t* button_x;

extern bool old_left_button_pressed;
extern bool old_right_button_pressed;

extern struct WindowObject* event_window;

extern struct list_node* icon_head;
extern struct list_node* icon_tail;

extern struct IconObject* dragged_icon;
extern int icon_drag_offset_x;
extern int icon_drag_offset_y;

extern struct IconObject* last_clicked_icon;
extern uint32_t last_icon_click_time;


void dwm_calculate_flush_region(struct window_context* ctx);
void dwm_draw_desktop(const struct window_context* ctx);
void dwm_draw_window(struct WindowObject* window_handle);
void dwm_update_mouse(struct window_context* ctx);
void dwm_update_window_dragging(struct window_context* ctx);
void dwm_set_focus(struct WindowObject* target);
void dwm_update_icon_dragging(struct window_context* ctx);
void dwm_invalidate_region(int16_t x, int16_t y, int16_t w, int16_t h);

void dwm_update(void) {
    window_context.left_button_pressed  = mouse_get_button(MOUSE_BUTTON_LEFT);
    window_context.right_button_pressed = mouse_get_button(MOUSE_BUTTON_RIGHT);
    
    if (dragged_window != NULL) {
        dwm_update_window_dragging(&window_context);
    } else if (dragged_icon != NULL) {
        dwm_update_icon_dragging(&window_context); // Added handler
    } else {
        dwm_update_mouse(&window_context);
    }
    
    // Recalculates screen bounds, looping through generic window nodes internally
    dwm_calculate_flush_region(&window_context);
    dwm_draw_desktop(&window_context);
    
    int final_min_x = (window_context.mouse.x < window_context.min_x) ? window_context.mouse.x : window_context.min_x;
    int final_min_y = (window_context.mouse.y < window_context.min_y) ? window_context.mouse.y : window_context.min_y;
    int final_max_x = (window_context.mouse.x + window_context.cursor_width > window_context.max_x) ? 
                       window_context.mouse.x + window_context.cursor_width : window_context.max_x;
    int final_max_y = (window_context.mouse.y + window_context.cursor_height > window_context.max_y) ? 
                       window_context.mouse.y + window_context.cursor_height : window_context.max_y;
    
    draw_flush_region(final_min_x, final_min_y, final_max_x - final_min_x, final_max_y - final_min_y);
    old_point = window_context.mouse;
    old_left_button_pressed = window_context.left_button_pressed;
    old_right_button_pressed = window_context.right_button_pressed;
    
    window_context.cursor_width   = cursor_width;
    window_context.cursor_height  = cursor_height;
    window_context.mouse = mouse_get_position();
    
    // Clear out both movement tracking flags for the next frame iteration
    window_context.window_moved = false;
    window_context.icon_moved = false; 
    window_context.old_win_min_x = 0; window_context.old_win_min_y = 0;
    window_context.old_win_max_x = 0; window_context.old_win_max_y = 0;
    window_context.old_icon_min_x = 0; window_context.old_icon_min_y = 0;
    window_context.old_icon_max_x = 0; window_context.old_icon_max_y = 0;
    
}

void dwm_draw_desktop(const struct window_context* ctx) {
    draw_rect_filled(old_point.x, old_point.y, ctx->cursor_width, ctx->cursor_height, bg_color);
    
    if (ctx->window_moved) {
        int16_t win_w = ctx->old_win_max_x - ctx->old_win_min_x;
        int16_t win_h = ctx->old_win_max_y - ctx->old_win_min_y;
        draw_rect_filled(ctx->old_win_min_x, ctx->old_win_min_y, win_w, win_h, bg_color);
    }
    
    if (ctx->icon_moved) {
        int16_t icon_w = ctx->old_icon_max_x - ctx->old_icon_min_x;
        int16_t icon_h = ctx->old_icon_max_y - ctx->old_icon_min_y;
        draw_rect_filled(ctx->old_icon_min_x, ctx->old_icon_min_y, icon_w, icon_h, bg_color);
    }
    
    // Icons
    
    struct list_node* current_node = icon_head;
    while (current_node != NULL) {
        struct IconObject* current_icon = (struct IconObject*)current_node->data;
        
        bool separate_x = (current_icon->x + current_icon->bounds_x >= ctx->max_x) || 
                          ((current_icon->x + current_icon->bounds_x + current_icon->bounds_w) <= ctx->min_x);
        bool separate_y = (current_icon->y + current_icon->bounds_y >= ctx->max_y) || 
                          ((current_icon->y + current_icon->bounds_y + current_icon->bounds_h) <= ctx->min_y);
        bool in_refresh_zone = !(separate_x || separate_y);
        
        // Redraw if inside the global damage evaluation area OR if anything moved
        if (in_refresh_zone || ctx->window_moved || ctx->icon_moved) {
            if (current_icon->icon_sprite != NULL) {
                draw_sprite(current_icon->icon_sprite, current_icon->width, current_icon->height, current_icon->x, current_icon->y, 0xFF000000);
            } else {
                draw_rect_filled(current_icon->x, current_icon->y, current_icon->width, current_icon->height, 0xFFFFFFFF);
            }
        }
        current_node = current_node->next;
    }
    
    // Windows
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        int win_min_x = window->x - window->border_width;
        int win_min_y = window->y - window->border_width;
        int win_max_x = window->x + window->w + window->border_width;
        int win_max_y = window->y + window->h + window->border_width;
        
        int separate_x = (win_min_x >= ctx->max_x) || (win_max_x <= ctx->min_x);
        int separate_y = (win_min_y >= ctx->max_y) || (win_max_y <= ctx->min_y);
        int do_redraw = !(separate_x || separate_y) || (window->flags & WINDOW_FLAG_REDRAW);
        
        if (!do_redraw) 
            continue;
        window->flags &= ~WINDOW_FLAG_REDRAW;
        
        dwm_draw_window(window);
        
        int clip_x = win_min_x + 1;
        int clip_y = win_min_y + window->titlebar_height + 1;
        int clip_w = win_max_x - win_min_x - 2;
        int clip_h = win_max_y - win_min_y - window->titlebar_height - 2;
        
        draw_set_clip_rect(clip_x, clip_y, clip_w, clip_h);
        draw_set_base_offset(clip_x, clip_y);
        
        event_window = window;
        
        if (window->event_callback != NULL) 
            window->event_callback(window->id, EVENT_REDRAW);
        
        event_window = NULL;
        
        draw_set_clip_rect(0, 0, display_get_width(), display_get_height());
        draw_set_base_offset(0, 0);
    }
    
    // Taskbar
    
    uint16_t taskbar_height = 28;
    uint16_t bar_x = 0;
    uint16_t bar_y = display_get_height() - taskbar_height;
    uint16_t bar_w = display_get_width();
    uint16_t bar_h = taskbar_height;
    
    uint32_t bar_color_low  = 0x00008000;
    uint32_t bar_color_high = 0x00001000;
    
    draw_rect_gradient_vertical(bar_x, bar_y, bar_w, bar_h, bar_color_low, bar_color_high);
    
    // Cursor
    
    draw_sprite(cursor_sprite, ctx->cursor_width, ctx->cursor_height, ctx->mouse.x, ctx->mouse.y, 0xFF000000);
}


void dwm_draw_window(struct WindowObject* window_handle) {
    int32_t wx = window_handle->x;
    int32_t wy = window_handle->y;
    int32_t ww = window_handle->w;
    int32_t wh = window_handle->h;
    
    // Cast window_tail->data back into a WindowObject configuration safely
    struct WindowObject* focused_window = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    
    uint32_t current_title_color_low = (window_handle == focused_window) ? 
                                        window_handle->title_color_low : 
                                        window_handle->inactive_color_low;
    
    uint32_t current_title_color_high = (window_handle == focused_window) ? 
                                         window_handle->title_color_high : 
                                         window_handle->inactive_color_high;
    
    draw_rect_filled(wx, wy + window_handle->titlebar_height, ww, wh - window_handle->titlebar_height, window_handle->background_color);
    
    if (window_handle->titlebar_height > 0) {
        draw_rect_gradient_vertical(wx, wy, ww, 
                                    window_handle->titlebar_height, 
                                    current_title_color_low, 
                                    current_title_color_high);
    }
    
    // Outer border
    for (uint8_t b = 1; b <= window_handle->border_width; b++) {
        draw_rect(
            window_handle->x - b, 
            window_handle->y - b, 
            window_handle->w + (b * 2), 
            window_handle->h + (b * 2), 
            window_handle->border_color
        );
    }
    
    // Title bar separator line
    for (uint8_t b = 0; b < window_handle->border_width; b++) {
        draw_line(
            window_handle->x, 
            window_handle->y + window_handle->titlebar_height + b - 1, 
            window_handle->x + window_handle->w, 
            window_handle->y + window_handle->titlebar_height + b - 1, 
            window_handle->border_color
        );
    }
    
    // Close button
    draw_sprite(
        button_x, 8, 8, 
        window_handle->x + window_handle->w - 15, 
        window_handle->y + (7 - (window_handle->border_width / 2) - 1), 
        0xFF000000
    );
}

void dwm_calculate_flush_region(struct window_context* ctx) {
    ctx->min_x = (old_point.x < ctx->mouse.x) ? old_point.x : ctx->mouse.x;
    ctx->min_y = (old_point.y < ctx->mouse.y) ? old_point.y : ctx->mouse.y;
    ctx->max_x = ((old_point.x > ctx->mouse.x) ? old_point.x : ctx->mouse.x) + ctx->cursor_width;
    ctx->max_y = ((old_point.y > ctx->mouse.y) ? old_point.y : ctx->mouse.y) + ctx->cursor_height;
    
    // Expand for windows
    if (ctx->window_moved) {
        if (ctx->old_win_min_x < ctx->min_x) ctx->min_x = ctx->old_win_min_x;
        if (ctx->old_win_min_y < ctx->min_y) ctx->min_y = ctx->old_win_min_y;
        if (ctx->old_win_max_x > ctx->max_x) ctx->max_x = ctx->old_win_max_x;
        if (ctx->old_win_max_y > ctx->max_y) ctx->max_y = ctx->old_win_max_y;
    }
    
    // Calculate icon bounds region
    if (ctx->icon_moved) {
        if (ctx->old_icon_min_x < ctx->min_x) ctx->min_x = ctx->old_icon_min_x;
        if (ctx->old_icon_min_y < ctx->min_y) ctx->min_y = ctx->old_icon_min_y;
        if (ctx->old_icon_max_x > ctx->max_x) ctx->max_x = ctx->old_icon_max_x;
        if (ctx->old_icon_max_y > ctx->max_y) ctx->max_y = ctx->old_icon_max_y;
    }
    
    // Calculate window bounds region
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        if (window->flags & WINDOW_FLAG_REDRAW) {
            int win_min_x = window->x - window->border_width;
            int win_min_y = window->y - window->border_width;
            int win_max_x = window->x + window->w + window->border_width;
            int win_max_y = window->y + window->h + window->border_width;
            
            if (win_min_x < ctx->min_x) ctx->min_x = win_min_x;
            if (win_min_y < ctx->min_y) ctx->min_y = win_min_y;
            if (win_max_x > ctx->max_x) ctx->max_x = win_max_x;
            if (win_max_y > ctx->max_y) ctx->max_y = win_max_y;
        }
    }
}

void dwm_update_icon_dragging(struct window_context* ctx) {
    if (dragged_icon == NULL) return;
    
    if (ctx->left_button_pressed) {
        int new_x = ctx->mouse.x - icon_drag_offset_x;
        int new_y = ctx->mouse.y - icon_drag_offset_y;
        int display_w = display_get_width();
        int display_h = display_get_height();
        
        // Clamp boundaries
        if (new_x + dragged_icon->bounds_x < 0)   
            new_x = -dragged_icon->bounds_x;
        if (new_x + dragged_icon->bounds_x + dragged_icon->bounds_w > display_w) 
            new_x = display_w - dragged_icon->bounds_x - dragged_icon->bounds_w;
        if (new_y + dragged_icon->bounds_y < 0) 
            new_y = -dragged_icon->bounds_y;
        if (new_y + dragged_icon->bounds_y + dragged_icon->bounds_h > display_h) 
            new_y = display_h - dragged_icon->bounds_y - dragged_icon->bounds_h;
        
        if (dragged_icon->x != new_x || dragged_icon->y != new_y) {
            // Log the tracking details explicitly to our NEW icon properties
            ctx->old_icon_min_x = dragged_icon->x + dragged_icon->bounds_x;
            ctx->old_icon_min_y = dragged_icon->y + dragged_icon->bounds_y;
            ctx->old_icon_max_x = dragged_icon->x + dragged_icon->bounds_x + dragged_icon->bounds_w;
            ctx->old_icon_max_y = dragged_icon->y + dragged_icon->bounds_y + dragged_icon->bounds_h;
            ctx->icon_moved = true;
            
            // Assign new positions
            dragged_icon->x = new_x;
            dragged_icon->y = new_y;
            
            // Invalidate windows located underneath the icon's new resting space
            dwm_draw_redraw(
                dragged_icon->x + dragged_icon->bounds_x, 
                dragged_icon->y + dragged_icon->bounds_y, 
                dragged_icon->bounds_w, 
                dragged_icon->bounds_h
            );
        }
    } else {
        // Drop item drop refresh invocation 
        ctx->old_icon_min_x = dragged_icon->x + dragged_icon->bounds_x;
        ctx->old_icon_min_y = dragged_icon->y + dragged_icon->bounds_y;
        ctx->old_icon_max_x = dragged_icon->x + dragged_icon->bounds_x + dragged_icon->bounds_w;
        ctx->old_icon_max_y = dragged_icon->y + dragged_icon->bounds_y + dragged_icon->bounds_h;
        ctx->icon_moved = true;
        
        dragged_icon = NULL;
    }
}

void dwm_update_window_dragging(struct window_context* ctx) {
    if (dragged_window == NULL) return;
    
    if (ctx->left_button_pressed) {
        int new_x = ctx->mouse.x - drag_offset_x;
        int new_y = ctx->mouse.y - drag_offset_y;
        
        int display_w = display_get_width();
        int display_h = display_get_height();
        
        if (new_x - dragged_window->border_width < 0)   
            new_x = dragged_window->border_width;
        if (new_x + dragged_window->w + dragged_window->border_width > display_w) 
            new_x = display_w - dragged_window->w - dragged_window->border_width;
            
        if (new_y - dragged_window->border_width < 0) 
            new_y = dragged_window->border_width;
        if (new_y + dragged_window->h + dragged_window->border_width > display_h) 
            new_y = display_h - dragged_window->h - dragged_window->border_width;
        
        if (dragged_window->x != new_x || dragged_window->y != new_y) {
            ctx->old_win_min_x = dragged_window->x - dragged_window->border_width;
            ctx->old_win_min_y = dragged_window->y - dragged_window->border_width;
            ctx->old_win_max_x = dragged_window->x + dragged_window->w + dragged_window->border_width;
            ctx->old_win_max_y = dragged_window->y + dragged_window->h + dragged_window->border_width;
            ctx->window_moved = true;
            
            dragged_window->x = new_x;
            dragged_window->y = new_y;
            
            dragged_window->surface_x = new_x;
            dragged_window->surface_y = new_y + dragged_window->titlebar_height + 1;
            dragged_window->surface_w = dragged_window->w;
            dragged_window->surface_h = dragged_window->h - dragged_window->titlebar_height;
            
            dragged_window->flags |= WINDOW_FLAG_REDRAW;
        }
    } else {
        dragged_window->flags |= WINDOW_FLAG_REDRAW;
        dragged_window = NULL;
    }
}

void dwm_update_mouse(struct window_context* ctx) {
    bool is_new_left_click = ctx->left_button_pressed && !old_left_button_pressed;
    bool is_new_right_click = ctx->right_button_pressed && !old_right_button_pressed;
    
    if (!is_new_left_click && !is_new_right_click) return;
    
    struct WindowObject* old_focused = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    struct WindowObject* clicked_win = NULL;
    
    // Check windows for mouse click targeting
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        int full_min_x = window->x - window->border_width;
        int full_max_x = window->x + window->w + window->border_width;
        int full_min_y = window->y - window->border_width;
        int full_max_y = window->y + window->h + window->border_width;
        
        if (ctx->mouse.x >= full_min_x && ctx->mouse.x <= full_max_x &&
            ctx->mouse.y >= full_min_y && ctx->mouse.y <= full_max_y) {
            clicked_win = window; 
        }
    }
    
    if (clicked_win != NULL) {
        dwm_set_focus(clicked_win);
        clicked_win->flags |= WINDOW_FLAG_REDRAW; 
        
        if (old_focused && old_focused != clicked_win) 
            old_focused->flags |= WINDOW_FLAG_REDRAW;
        
        int title_min_x = clicked_win->x;
        int title_max_x = clicked_win->x + clicked_win->w;
        int title_min_y = clicked_win->y;
        int title_max_y = clicked_win->y + clicked_win->titlebar_height - 1;
        
        if (ctx->mouse.x >= title_min_x && ctx->mouse.x <= title_max_x &&
            ctx->mouse.y >= title_min_y && ctx->mouse.y <= title_max_y) {
            
            if (is_new_left_click) {
                dragged_window = clicked_win;
                drag_offset_x = ctx->mouse.x - clicked_win->x;
                drag_offset_y = ctx->mouse.y - clicked_win->y;
            } 
            else if (is_new_right_click) {
                destroy_window((WindowHandle)clicked_win->id);
            }
        }
    } else {
        
        // Check desktop icon clicks
        
        struct IconObject* clicked_icon = NULL;
        
        for (struct list_node* node = icon_head; node != NULL; node = node->next) {
            struct IconObject* icon = (struct IconObject*)node->data;
            
            int icon_min_x = icon->x + icon->bounds_x;
            int icon_max_x = icon->x + icon->bounds_x + icon->bounds_w;
            int icon_min_y = icon->y + icon->bounds_y;
            int icon_max_y = icon->y + icon->bounds_y + icon->bounds_h;
            
            if (ctx->mouse.x >= icon_min_x && ctx->mouse.x <= icon_max_x &&
                ctx->mouse.y >= icon_min_y && ctx->mouse.y <= icon_max_y) {
                clicked_icon = icon;
                break; // Found the top icon, stop scanning
            }
        }
        
        if (clicked_icon != NULL) {
            if (is_new_left_click) {
                uint32_t current_time = timer_get_ms();
                
                // NEW: Evaluate Double Click condition
                if (clicked_icon == last_clicked_icon && 
                    (current_time - last_icon_click_time) <= DOUBLE_CLICK_THRESHOLD_MS) {
                    
                    // --- DOUBLE CLICK EVENT DETECTED ---
                    // TODO: Route this to your upcoming event system!
                    // Example: kernel_raise_icon_event(clicked_icon, EVENT_ICON_DOUBLE_CLICK);
                    
                    
                    destroy_icon(clicked_icon);
                    
                    
                    // Clear state so a rapid 3rd click doesn't trigger a 2nd consecutive double-click
                    last_clicked_icon = NULL;
                    last_icon_click_time = 0;
                    
                } else {
                    // This is treated as a regular Single Click / Start Dragging action
                    dragged_icon = clicked_icon;
                    icon_drag_offset_x = ctx->mouse.x - clicked_icon->x;
                    icon_drag_offset_y = ctx->mouse.y - clicked_icon->y;
                    
                    // Save this icon and timestamp to check against on the next click
                    last_clicked_icon = clicked_icon;
                    last_icon_click_time = current_time;
                }
            } 
            else if (is_new_right_click) {
                destroy_icon(clicked_icon);
                last_clicked_icon = NULL; // Clear state if destroyed
            }
        } else {
            // User clicked completely blank desktop space
            if (is_new_left_click) {
                last_clicked_icon = NULL;
            }
        }
    }
}
