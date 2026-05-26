#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>

extern uint32_t bg_color;

extern struct WindowHandle* dragged_window;
extern int drag_offset_x;
extern int drag_offset_y;

extern struct WindowHandle* window_head;
extern struct WindowHandle* window_tail;

extern struct WindowContext windowContext;
extern Point old_point;

extern uint32_t* cursor_sprite;
extern uint32_t* button_x;

extern bool old_left_button_pressed;
extern bool old_right_button_pressed;

extern struct WindowContext windowContext;

void handle_window_dragging(struct WindowContext *ctx);
void dwm_calculate_flush_boundary(struct WindowContext *ctx);
void dwm_render_desktop(const struct WindowContext *ctx);
void dwm_render_window(struct WindowHandle* window_handle);
void handle_mouse_clicks(struct WindowContext *ctx);


void dwm_update(void) {
    windowContext.left_button_pressed  = mouse_get_button(MOUSE_BUTTON_LEFT);
    windowContext.right_button_pressed = mouse_get_button(MOUSE_BUTTON_RIGHT);
    
    if (dragged_window != NULL) {
        handle_window_dragging(&windowContext);
    } else {
        handle_mouse_clicks(&windowContext);
    }
    
    dwm_calculate_flush_boundary(&windowContext);
    dwm_render_desktop(&windowContext);
    
    int final_min_x = (windowContext.point.x < windowContext.min_x) ? windowContext.point.x : windowContext.min_x;
    int final_min_y = (windowContext.point.y < windowContext.min_y) ? windowContext.point.y : windowContext.min_y;
    int final_max_x = (windowContext.point.x + windowContext.cursor_width > windowContext.max_x) ? 
                       windowContext.point.x + windowContext.cursor_width : windowContext.max_x;
    int final_max_y = (windowContext.point.y + windowContext.cursor_height > windowContext.max_y) ? 
                       windowContext.point.y + windowContext.cursor_height : windowContext.max_y;
    
    draw_flush_region(final_min_x, final_min_y, final_max_x - final_min_x, final_max_y - final_min_y);
    
    old_point = windowContext.point;
    old_left_button_pressed = windowContext.left_button_pressed;
    old_right_button_pressed = windowContext.right_button_pressed;
    
    windowContext.cursor_width   = 11;
    windowContext.cursor_height  = 11;
    windowContext.point = mouse_get_position();
    windowContext.window_moved = false;
    windowContext.old_win_min_x = 0;
    windowContext.old_win_min_y = 0;
    windowContext.old_win_max_x = 0;
    windowContext.old_win_max_y = 0;
    
}

void dwm_render_desktop(const struct WindowContext* ctx) {
    // Blank the old cursor region
    draw_rect_filled(old_point.x, old_point.y, ctx->cursor_width, ctx->cursor_height, bg_color);
    
    if (ctx->window_moved) {
        int16_t win_x = ctx->old_win_min_x;
        int16_t win_y = ctx->old_win_min_y;
        int16_t win_w = ctx->old_win_max_x - ctx->old_win_min_x;
        int16_t win_h = ctx->old_win_max_y - ctx->old_win_min_y;
        
        draw_rect_filled(win_x, win_y, win_w, win_h, bg_color);
    }
    
    // Redraw windows that intersect with the clipping region
    
    for (struct WindowHandle* current = window_head; current != NULL; current = current->next) {
        int win_min_x = current->x - current->border_width;
        int win_min_y = current->y - current->border_width;
        int win_max_x = current->x + current->w + current->border_width;
        int win_max_y = current->y + current->h + current->border_width;
        
        int separate_x = (win_min_x >= ctx->max_x) || (win_max_x <= ctx->min_x);
        int separate_y = (win_min_y >= ctx->max_y) || (win_max_y <= ctx->min_y);
        int do_redraw = !(separate_x || separate_y) || (current->flags & WINDOW_FLAG_REDRAW);
        
        if (!do_redraw) 
            continue;
        current->flags &= ~WINDOW_FLAG_REDRAW;
        
        // Draw window bordering and title
        dwm_render_window(current);
        
        int clip_x = win_min_x + 1;
        int clip_y = win_min_y + current->titlebar_height + 2;
        int clip_w = win_max_x - win_min_x - 2;
        int clip_h = win_max_y - win_min_y - current->titlebar_height - 3;
         
        draw_set_clip_rect(clip_x, clip_y, clip_w, clip_h);
        
        if (current->event_callback != NULL) 
            current->event_callback(current);
        
        draw_set_clip_rect(0, 0, display_get_width(), display_get_height());
    }
    
    // Draw taskbar
    
    uint16_t taskbar_height = 28;
    uint16_t bar_x = 0;
    uint16_t bar_y = display_get_height() - taskbar_height;
    uint16_t bar_w = display_get_width();
    uint16_t bar_h = taskbar_height;
    
    uint32_t bar_color_low  = 0x00008000;
    uint32_t bar_color_high = 0x00001000;
    
    //draw_rect_gradient_vertical(200, 200, 500, 500, 0xFF010101, 0xFFF0F0F0);
    //draw_rect_gradient_vertical(700, 200, 50, 50, 0xFF010101, 0xFFF0F0F0);
    //draw_rect_gradient_vertical_offset(200, 200, 500, 50, 0xFF010101, 0xFFF0F0F0, -40);
    
    draw_rect_gradient_vertical_offset(bar_x, bar_y, bar_w, bar_h, bar_color_low, bar_color_high, 0);
    
    //draw_rect_gradient_vertical(bar_x, bar_y, bar_w, bar_h, bar_color_low, bar_color_high);
    
    
    
    
    // Render the cursor sprite
    
    draw_sprite(cursor_sprite, ctx->cursor_width, ctx->cursor_height, ctx->point.x, ctx->point.y, 0xFF000000);
    
    
    
    
}


void dwm_render_window(struct WindowHandle* window_handle) {
    int32_t wx = window_handle->x;
    int32_t wy = window_handle->y;
    int32_t ww = window_handle->w;
    int32_t wh = window_handle->h;
    
    struct WindowHandle* focused_window = window_tail;
    
    uint32_t current_title_color_low = (window_handle == focused_window) ? 
                                        window_handle->title_color_low : 
                                        window_handle->inactive_color_low;
    
    uint32_t current_title_color_high = (window_handle == focused_window) ? 
                                         window_handle->title_color_high : 
                                         window_handle->inactive_color_high;
    
    // Fill client window background color (Offset below titlebar)
    draw_rect_filled(wx, wy + window_handle->titlebar_height, ww, wh - window_handle->titlebar_height, window_handle->background_color);
    
    // Title bar fill color (Starts exactly at wy)
    if (window_handle->titlebar_height > 0) {
        
        draw_rect_gradient_vertical(wx, wy, ww, 
                                    window_handle->titlebar_height, 
                                    current_title_color_low, 
                                    current_title_color_high);
        
    }
    
    // Outer Frame Border
    for (uint8_t b = 1; b <= window_handle->border_width; b++) {
        draw_rect(
            window_handle->x - b, 
            window_handle->y - b, 
            window_handle->w + (b * 2), 
            window_handle->h + (b * 2), 
            window_handle->border_color
        );
    }
    
    // Title bar separator line (Drawn directly under titlebar canvas)
    for (uint8_t b = 0; b < window_handle->border_width; b++) {
        draw_line(
            window_handle->x, 
            window_handle->y + window_handle->titlebar_height + b, 
            window_handle->x + window_handle->w, 
            window_handle->y + window_handle->titlebar_height + b, 
            window_handle->border_color
        );
    }
    
    // Close button positioning relative to top titlebar space
    draw_sprite(
        button_x, 8, 8, 
        window_handle->x + window_handle->w - 15, 
        window_handle->y + (7 - (window_handle->border_width / 2) - 1), 
        0xFF000000
    );
}

void handle_window_dragging(struct WindowContext *ctx) {
    if (dragged_window == NULL) return;
    
    if (ctx->left_button_pressed) {
        int new_x = ctx->point.x - drag_offset_x;
        int new_y = ctx->point.y - drag_offset_y;
        
        int display_w = display_get_width();
        int display_h = display_get_height();
        
        // Edge calculation matches simplified bounding boxes
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
            
            // Update the user draw space metrics
            dragged_window->surface_x = new_x;
            dragged_window->surface_y = new_y + dragged_window->titlebar_height + 1;
            dragged_window->surface_w = dragged_window->w;
            dragged_window->surface_h = dragged_window->h - dragged_window->titlebar_height;
            
            dragged_window->flags |= WINDOW_FLAG_REDRAW;
        }
    } else {
        dragged_window = NULL;
    }
}

void dwm_calculate_flush_boundary(struct WindowContext *ctx) {
    ctx->min_x = (old_point.x < ctx->point.x) ? old_point.x : ctx->point.x;
    ctx->min_y = (old_point.y < ctx->point.y) ? old_point.y : ctx->point.y;
    ctx->max_x = ((old_point.x > ctx->point.x) ? old_point.x : ctx->point.x) + ctx->cursor_width;
    ctx->max_y = ((old_point.y > ctx->point.y) ? old_point.y : ctx->point.y) + ctx->cursor_height;
    
    if (ctx->window_moved) {
        if (ctx->old_win_min_x < ctx->min_x) ctx->min_x = ctx->old_win_min_x;
        if (ctx->old_win_min_y < ctx->min_y) ctx->min_y = ctx->old_win_min_y;
        if (ctx->old_win_max_x > ctx->max_x) ctx->max_x = ctx->old_win_max_x;
        if (ctx->old_win_max_y > ctx->max_y) ctx->max_y = ctx->old_win_max_y;
    }
    
    for (struct WindowHandle* curr = window_head; curr != NULL; curr = curr->next) {
        if (curr->flags & WINDOW_FLAG_REDRAW) {
            int win_min_x = curr->x - curr->border_width;
            int win_min_y = curr->y - curr->border_width;
            int win_max_x = curr->x + curr->w + curr->border_width;
            int win_max_y = curr->y + curr->h + curr->border_width;
            
            if (win_min_x < ctx->min_x) ctx->min_x = win_min_x;
            if (win_min_y < ctx->min_y) ctx->min_y = win_min_y;
            if (win_max_x > ctx->max_x) ctx->max_x = win_max_x;
            if (win_max_y > ctx->max_y) ctx->max_y = win_max_y;
        }
    }
}

void handle_mouse_clicks(struct WindowContext *ctx) {
    bool is_new_left_click = ctx->left_button_pressed && !old_left_button_pressed;
    bool is_new_right_click = ctx->right_button_pressed && !old_right_button_pressed;

    if (!is_new_left_click && !is_new_right_click) return;
    
    struct WindowHandle* old_focused = window_tail;
    struct WindowHandle* clicked_win = NULL;
    
    for (struct WindowHandle* curr = window_head; curr != NULL; curr = curr->next) {
        int full_min_x = curr->x - curr->border_width;
        int full_max_x = curr->x + curr->w + curr->border_width;
        int full_min_y = curr->y - curr->border_width;
        int full_max_y = curr->y + curr->h + curr->border_width;
        
        if (ctx->point.x >= full_min_x && ctx->point.x <= full_max_x &&
            ctx->point.y >= full_min_y && ctx->point.y <= full_max_y) {
            clicked_win = curr; 
        }
    }
    
    if (clicked_win != NULL) {
        dwm_set_focus(clicked_win);
        clicked_win->flags |= WINDOW_FLAG_REDRAW; 
        
        if (old_focused && old_focused != clicked_win) {
            old_focused->flags |= WINDOW_FLAG_REDRAW;
        }
        
        // Define titlebar space boundaries explicitly
        int title_min_x = clicked_win->x;
        int title_max_x = clicked_win->x + clicked_win->w;
        int title_min_y = clicked_win->y;
        int title_max_y = clicked_win->y + clicked_win->titlebar_height;
        
        if (ctx->point.x >= title_min_x && ctx->point.x <= title_max_x &&
            ctx->point.y >= title_min_y && ctx->point.y <= title_max_y) {
            
            if (is_new_left_click) {
                dragged_window = clicked_win;
                drag_offset_x = ctx->point.x - clicked_win->x;
                drag_offset_y = ctx->point.y - clicked_win->y;
            } 
            else if (is_new_right_click) {
                destroy_window(clicked_win);
            }
        }
    }
}

