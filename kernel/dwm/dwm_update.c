#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

void dwm_update(void) {
    context.window_context.left_button_pressed  = mouse_get_button(MOUSE_BUTTON_LEFT);
    context.window_context.right_button_pressed = mouse_get_button(MOUSE_BUTTON_RIGHT);
    context.window_context.mouse = mouse_get_position();
    
    // Invalidate old mouse position.
    dwm_invalidate_region(input.mouse_last.x, input.mouse_last.y, context.window_context.cursor_width, context.window_context.cursor_height);
    
    // UI processing loops
    if (dragdrop.dragged_window != NULL) {
        dwm_update_window_dragging(&context.window_context);
    } else if (dragdrop.dragged_resizing != NULL) {
        dwm_update_window_resizing(&context.window_context);
    } else if (dragdrop.dragged_icon != NULL) {
        dwm_update_icon_dragging(&context.window_context);
    } else {
        dwm_update_mouse(&context.window_context);
    }
    
    // Deferred deletion
    
    struct list_node* current = workspace.window_head;
    while (current != NULL) {
        // Save the next pointer immediately before destruction breaks it
        struct list_node* next_node = current->next;
        struct WindowObject* window = (struct WindowObject*)current->data;
        
        if (window->events & DWM_EVENT_CLOSE) {
            
            window->event_callback(window->id, DWM_EVENT_DESTROY, 0, 0);
            
            dwm_destroy_window(window->id);
        }
        
        current = next_node;
    }
    
    // Invalidate new mouse position
    dwm_invalidate_region(context.window_context.mouse.x, context.window_context.mouse.y, context.window_context.cursor_width, context.window_context.cursor_height);
    
    // Draw remaining window elements over background
    dwm_draw_desktop(&context.window_context);
    
    // Flush everything to the front
    for (int i = 0; i < context.window_context.dirty_count; i++) {
        struct Rect r = context.window_context.dirty_regions[i];
        draw_flush_region(r.x, r.y, r.w, r.h);
    }
    
    // Reset frame state
    
    input.mouse_last = context.window_context.mouse;
    input.last_left_button_pressed = context.window_context.left_button_pressed;
    input.last_right_button_pressed = context.window_context.right_button_pressed;
    context.window_context.cursor_width   = images.current_cursor.width;
    context.window_context.cursor_height  = images.current_cursor.height;
    context.window_context.dirty_count = 0;
}
