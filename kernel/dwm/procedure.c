#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>

#include <kernel/console/display.h>

void callback_button_close_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    switch (event) {
    case DWM_EVENT_MOUSE:
        WindowHandle parent = dwm_window_get_parent(handle);
        dwm_window_send_event(parent, DWM_EVENT_CLOSE);
        break;
        
    case DWM_EVENT_REDRAW:
        dwm_draw_sprite(0, 0, dwm_resource_find("button_close"));
        break;
    }
}

void callback_taskbar_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    switch (event) {
    case DWM_EVENT_MOUSE:
        break;
        
    case DWM_EVENT_REDRAW:
        dwm_draw_rect_filled_gradient(0, -3, display_get_width(), taskbar_height + 7, 0xFF000000, 0xFF00C000);
        break;
    }
}

void callback_message_box_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    // Define explicit button dimensions since we aren't using a sprite asset anymore
    uint16_t button_width  = 60;
    uint16_t button_height = 24;
    
    // Dynamically calculate the centered x-coordinate based on the window width
    uint16_t button_x = (window->w - button_width) / 2;
    uint16_t button_y = ((window->h - button_height) - button_height) - 7;
    
    switch (event) {
        case DWM_EVENT_REDRAW:
            // Window background
            dwm_draw_rect_filled(0, 0, window->w, window->h, 0xFF08080F);
            
            // Message text
            char* message_resource = (char*)dwm_window_resource_get_by_name(handle, "text");
            if (message_resource != NULL) 
                dwm_draw_text(15, 20, message_resource, window->title_text_color);
            
            // Draw button body
            dwm_draw_rect_filled(button_x, button_y, button_width, button_height, 0xFF1C1C2A);
            dwm_draw_rect(button_x, button_y, button_width, button_height, 0xFF444466);
            
            // Center the text inside the procedurally drawn button
            const char* button_text = "ok"; 
            uint16_t text_width = strlen(button_text) * 6; // 6px per character
            
            uint16_t text_x = button_x + ((button_width - text_width) / 2);
            uint16_t text_y = button_y + ((button_height - 8) / 2); // Center 8px font vertically
            
            dwm_draw_text(text_x, text_y, button_text, 0xFFC0FFC0);
            
            break;
            
        case DWM_EVENT_MOUSE: {
            uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
            uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
            
            // Hit-test against the procedural button boundaries
            if (click_x >= button_x && click_x < (button_x + button_width) && 
                click_y >= button_y && click_y < (button_y + button_height)) {
                
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            }
            break;
        }
            
        default:
            break;
    }
}
