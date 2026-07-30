#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

void dwm_update(void) {
    // Process Window Messages
    DWMMessage msg;
    while (dwm_get_message(&msg)) {
        dwm_dispatch_message(&msg);
    }
    
    // Invalidate the OLD cursor position
    dwm_invalidate_region(input.mouse_last.x, input.mouse_last.y, context.window_context.cursor_width, context.window_context.cursor_height);
    
    // Process Hardware Input Queue
    MouseEvent m_event;
    
    // Drain the queue of all events that happened since the last tick
    while (mouse_dequeue_event(&m_event)) {
        
        // Update context based on this specific snapshot in time
        context.window_context.mouse.x = m_event.x;
        context.window_context.mouse.y = m_event.y;
        context.window_context.left_button_pressed  = m_event.left_button;
        context.window_context.right_button_pressed = m_event.right_button;
        
        // Update UI logic for THIS specific event
        if (dragdrop.dragged_window != NULL) {
            dwm_update_window_dragging(&context.window_context);
        } else if (dragdrop.dragged_resizing != NULL) {
            dwm_update_window_resizing(&context.window_context);
        } else if (dragdrop.dragged_icon != NULL) {
            dwm_update_icon_dragging(&context.window_context);
        } else {
            dwm_update_mouse(&context.window_context); 
        }
    }
    
    // Invalidate the new cursor position
    dwm_invalidate_region(context.window_context.mouse.x, context.window_context.mouse.y, context.window_context.cursor_width, context.window_context.cursor_height);
    
    // Draw and flush the screen
    dwm_draw_desktop(&context.window_context);
    
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
