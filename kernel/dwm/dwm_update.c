#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>

#include <kernel/console/display.h>
#include <kernel/dwm/icon_object.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>

#include <kernel/util/string.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

#include <kernel/dwm/dwm_core_internal.h>

void dwm_update(void) {
    window_context.left_button_pressed  = mouse_get_button(MOUSE_BUTTON_LEFT);
    window_context.right_button_pressed = mouse_get_button(MOUSE_BUTTON_RIGHT);
    
    window_context.menu_moved = false;
    
    // Mouse hover tracking
    if (context_menu.visible) {
        int old_hover = context_menu.hovered_item;
        context_menu.hovered_item = -1; // Reset default
        
        // Check if mouse is within the context menu bounds
        if (window_context.mouse.x >= context_menu.x && 
            window_context.mouse.x < (context_menu.x + context_menu.w) &&
            window_context.mouse.y >= context_menu.y && 
            window_context.mouse.y < (context_menu.y + context_menu.h)) {
            
            // Calculate which item index the mouse is over
            int relative_y = window_context.mouse.y - context_menu.y;
            int item_index = relative_y / context_menu.item_height;
            
            if (item_index >= 0 && item_index < (int)context_menu.item_count) {
                context_menu.hovered_item = item_index;
            }
        }
        
        // If the hovered item changed, we must trigger a redraw of the menu area
        if (context_menu.hovered_item != old_hover) {
            window_context.menu_moved = true;
            window_context.old_menu_min_x = context_menu.x;
            window_context.old_menu_min_y = context_menu.y;
            window_context.old_menu_max_x = context_menu.x + context_menu.w;
            window_context.old_menu_max_y = context_menu.y + context_menu.h;
        }
    }
    
    if (dragged_window != NULL) {
        dwm_update_window_dragging(&window_context);
    } else if (dragged_icon != NULL) {
        dwm_update_icon_dragging(&window_context);
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
    mouse_old = window_context.mouse;
    
    old_left_button_pressed = window_context.left_button_pressed;
    old_right_button_pressed = window_context.right_button_pressed;
    
    window_context.cursor_width   = cursor.width;
    window_context.cursor_height  = cursor.height;
    window_context.mouse = mouse_get_position();
    
    // Clear out movement tracking flags for the next frame iteration
    window_context.window_moved = false;
    window_context.icon_moved = false; 
    window_context.old_win_min_x = 0; window_context.old_win_min_y = 0;
    window_context.old_win_max_x = 0; window_context.old_win_max_y = 0;
    window_context.old_icon_min_x = 0; window_context.old_icon_min_y = 0;
    window_context.old_icon_max_x = 0; window_context.old_icon_max_y = 0;
    window_context.old_menu_min_x = 0; window_context.old_menu_min_y = 0;
    window_context.old_menu_max_x = 0; window_context.old_menu_max_y = 0;
}

void dwm_calculate_flush_region(struct WindowContext* ctx) {
    ctx->min_x = (mouse_old.x < ctx->mouse.x) ? mouse_old.x : ctx->mouse.x;
    ctx->min_y = (mouse_old.y < ctx->mouse.y) ? mouse_old.y : ctx->mouse.y;
    ctx->max_x = ((mouse_old.x > ctx->mouse.x) ? mouse_old.x : ctx->mouse.x) + ctx->cursor_width;
    ctx->max_y = ((mouse_old.y > ctx->mouse.y) ? mouse_old.y : ctx->mouse.y) + ctx->cursor_height;
    
    if (ctx->window_moved) {
        if (ctx->old_win_min_x < ctx->min_x) ctx->min_x = ctx->old_win_min_x;
        if (ctx->old_win_min_y < ctx->min_y) ctx->min_y = ctx->old_win_min_y;
        if (ctx->old_win_max_x > ctx->max_x) ctx->max_x = ctx->old_win_max_x;
        if (ctx->old_win_max_y > ctx->max_y) ctx->max_y = ctx->old_win_max_y;
    }
    
    if (ctx->icon_moved) {
        if (ctx->old_icon_min_x < ctx->min_x) ctx->min_x = ctx->old_icon_min_x;
        if (ctx->old_icon_min_y < ctx->min_y) ctx->min_y = ctx->old_icon_min_y;
        if (ctx->old_icon_max_x > ctx->max_x) ctx->max_x = ctx->old_icon_max_x;
        if (ctx->old_icon_max_y > ctx->max_y) ctx->max_y = ctx->old_icon_max_y;
    }
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        if ((window->flags & WINDOW_FLAG_REDRAW) || (window == dragged_window)) {
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
    
    if (context_menu.visible) {
        if (context_menu.x < ctx->min_x) ctx->min_x = context_menu.x;
        if (context_menu.y < ctx->min_y) ctx->min_y = context_menu.y;
        if ((context_menu.x + context_menu.w) > ctx->max_x) ctx->max_x = (context_menu.x + context_menu.w);
        if ((context_menu.y + context_menu.h) > ctx->max_y) ctx->max_y = (context_menu.y + context_menu.h);
    }
    
    if (ctx->menu_moved) {
        if (ctx->old_menu_min_x < ctx->min_x) ctx->min_x = ctx->old_menu_min_x;
        if (ctx->old_menu_min_y < ctx->min_y) ctx->min_y = ctx->old_menu_min_y;
        if (ctx->old_menu_max_x > ctx->max_x) ctx->max_x = ctx->old_menu_max_x;
        if (ctx->old_menu_max_y > ctx->max_y) ctx->max_y = ctx->old_menu_max_y;
    }
    
    uint16_t display_h = display_get_height();
    uint16_t taskbar_y = display_h - taskbar_height;
    
    if (ctx->max_y >= taskbar_y) {
        if (0 < ctx->min_x) ctx->min_x = 0;
        if (taskbar_y < ctx->min_y) ctx->min_y = taskbar_y;
        
        uint16_t display_w = display_get_width();
        if (display_w > ctx->max_x) ctx->max_x = display_w;
        if (display_h > ctx->max_y) ctx->max_y = display_h;
    }
}
