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
    window_context.mouse = mouse_get_position();
    
    // Invalidate the old mouse position to clear the ghost cursor
    dwm_invalidate_region(mouse_old.x, mouse_old.y, window_context.cursor_width, window_context.cursor_height);
    
    if (context_menu.visible) {
        int old_hover = context_menu.hovered_item;
        context_menu.hovered_item = -1;
        
        if (window_context.mouse.x >= context_menu.x && 
            window_context.mouse.x < (context_menu.x + context_menu.w) &&
            window_context.mouse.y >= context_menu.y && 
            window_context.mouse.y < (context_menu.y + context_menu.h)) {
            
            int relative_y = window_context.mouse.y - context_menu.y;
            int item_index = relative_y / context_menu.item_height;
            
            if (item_index >= 0 && item_index < (int)context_menu.item_count) {
                context_menu.hovered_item = item_index;
            }
        }
        
        if (context_menu.hovered_item != old_hover) {
            dwm_invalidate_region(context_menu.x, context_menu.y, context_menu.w, context_menu.h);
        }
    }
    
    if (dragged_window != NULL) {
        dwm_update_window_dragging(&window_context);
    } else if (dragged_icon != NULL) {
        dwm_update_icon_dragging(&window_context);
    } else {
        dwm_update_mouse(&window_context);
    }
    
    // Invalidate the new mouse position so it renders over the background
    dwm_invalidate_region(window_context.mouse.x, window_context.mouse.y, window_context.cursor_width, window_context.cursor_height);
    
    dwm_draw_desktop(&window_context);
    
    // Flush all tracked regions to the hardware display
    for (int i = 0; i < window_context.dirty_count; i++) {
        struct Rect r = window_context.dirty_regions[i];
        draw_flush_region(r.x, r.y, r.w, r.h);
    }
    
    mouse_old = window_context.mouse;
    old_left_button_pressed = window_context.left_button_pressed;
    old_right_button_pressed = window_context.right_button_pressed;
    
    window_context.cursor_width   = cursor.width;
    window_context.cursor_height  = cursor.height;
    
    // Wipe the damage array for the next frame
    window_context.dirty_count = 0;
}
