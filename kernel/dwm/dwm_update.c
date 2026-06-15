#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

void dwm_update(void) {
    window_context.left_button_pressed  = mouse_get_button(MOUSE_BUTTON_LEFT);
    window_context.right_button_pressed = mouse_get_button(MOUSE_BUTTON_RIGHT);
    window_context.mouse = mouse_get_position();
    
    // Invalidate old mouse position.
    dwm_invalidate_region(mouse_old.x, mouse_old.y, window_context.cursor_width, window_context.cursor_height);
    
    // UI processing loops
    if (dragged_window != NULL) {
        dwm_update_window_dragging(&window_context);
    } else if (resizing_window != NULL) {
        dwm_update_window_resizing(&window_context);
    } else if (dragged_icon != NULL) {
        dwm_update_icon_dragging(&window_context);
    } else {
        dwm_update_mouse(&window_context);
    }
    
    // Deferred deletion
    
    struct list_node* current = window_head;
    while (current != NULL) {
        // Save the next pointer immediately before destruction breaks it
        struct list_node* next_node = current->next;
        struct WindowObject* window = (struct WindowObject*)current->data;
        
        if (window->events & EVENT_CLOSE) {
            
            window->event_callback(window->id, EVENT_DESTROY, 0, 0);
            
            dwm_destroy_window(window->id);
        }
        
        current = next_node;
    }
    
    // Invalidate new mouse position
    dwm_invalidate_region(window_context.mouse.x, window_context.mouse.y, window_context.cursor_width, window_context.cursor_height);
    
    // Draw remaining window elements over background
    dwm_draw_desktop(&window_context);
    
    // Flush everything to the front
    for (int i = 0; i < window_context.dirty_count; i++) {
        struct Rect r = window_context.dirty_regions[i];
        draw_flush_region(r.x, r.y, r.w, r.h);
    }
    
    // Reset frame state
    
    mouse_old = window_context.mouse;
    old_left_button_pressed = window_context.left_button_pressed;
    old_right_button_pressed = window_context.right_button_pressed;
    window_context.cursor_width   = current_cursor.width;
    window_context.cursor_height  = current_cursor.height;
    window_context.dirty_count = 0;
}
