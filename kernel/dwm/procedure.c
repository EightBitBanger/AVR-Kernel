#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>

#include <kernel/console/display.h>

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


void callback_properties_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    uint16_t tab_width  = 80;
    uint16_t tab_height = 24;
    uint16_t tab_gap    = 6; // Space between each tab (set to 0 if you want them side-by-side)
    
    uint16_t tab_general_x = 30; // Starting X position for the first tab
    uint16_t tab_general_y = 5;
    
    // Automatically calculate positions to avoid overlaps
    uint16_t tab_attrib_x = tab_general_x + tab_width + tab_gap;
    uint16_t tab_attrib_y = 5;
    
    uint16_t tab_perm_x = tab_attrib_x + tab_width + tab_gap;
    uint16_t tab_perm_y = 5;
    
    // Calculate horizontal center by window width
    uint16_t button_width  = 60;
    uint16_t button_height = 24;
    
    uint16_t button_x = window->w - (button_width + (button_width / 2));
    uint16_t button_y = ((window->h - button_height) - button_height) - 7;
    
    switch (event) {
        case DWM_EVENT_REDRAW:
            // Window background
            dwm_draw_rect_filled(0, 0, window->w, window->h, 0xFF08080F);
            
            dwm_draw_line(0, 24, window->w, 0, 0xFF04C004);
            
            char* path_str = (char*)dwm_window_resource_get_by_name(handle, "path");
            if (path_str != NULL) 
                dwm_draw_text(15, 30, path_str, 0xFFFFFFFF);
            
            char* path_addr = (char*)dwm_window_resource_get_by_name(handle, "name");
            if (path_addr != NULL) 
                dwm_draw_text(15, 50, path_addr, 0xFFFFFFFF);
            
            // Tab 'general'
            {
            dwm_draw_rect_filled(tab_general_x, tab_general_y, tab_width, tab_height, 0xFF1C1C2A);
            dwm_draw_rect(tab_general_x, tab_general_y, tab_width, tab_height, 0xFF444466);
            
            const char* button_text = "General"; 
            uint16_t text_width = strlen(button_text) * 6; // 6px per character
            
            uint16_t text_x = tab_general_x + ((tab_width - text_width) / 2);
            uint16_t text_y = tab_general_y + ((tab_height - 8) / 2); // Center 8px font vertically
            
            dwm_draw_text(text_x, text_y, button_text, 0xFFC0FFC0);
            }
            
            // Tab 'attributes'
            {
            dwm_draw_rect_filled(tab_attrib_x, tab_attrib_y, tab_width, tab_height, 0xFF1C1C2A);
            dwm_draw_rect(tab_attrib_x, tab_attrib_y, tab_width, tab_height, 0xFF444466);
            
            const char* button_text = "attributes"; 
            uint16_t text_width = strlen(button_text) * 6; // 6px per character
            
            uint16_t text_x = tab_attrib_x + ((tab_width - text_width) / 2);
            uint16_t text_y = tab_attrib_y + ((tab_height - 8) / 2); // Center 8px font vertically
            
            dwm_draw_text(text_x, text_y, button_text, 0xFFC0FFC0);
            }
            
            // Tab 'permissions'
            {
            dwm_draw_rect_filled(tab_perm_x, tab_perm_y, tab_width, tab_height, 0xFF1C1C2A);
            dwm_draw_rect(tab_perm_x, tab_perm_y, tab_width, tab_height, 0xFF444466);
            
            const char* button_text = "permissions"; 
            uint16_t text_width = strlen(button_text) * 6; // 6px per character
            
            uint16_t text_x = tab_perm_x + ((tab_width - text_width) / 2);
            uint16_t text_y = tab_perm_y + ((tab_height - 8) / 2); // Center 8px font vertically
            
            dwm_draw_text(text_x, text_y, button_text, 0xFFC0FFC0);
            }
            
            // Draw button body
            {
            dwm_draw_rect_filled(button_x, button_y, button_width, button_height, 0xFF1C1C2A);
            dwm_draw_rect(button_x, button_y, button_width, button_height, 0xFF444466);
            
            const char* button_text = "close"; 
            uint16_t text_width = strlen(button_text) * 6; // 6px per character
            
            uint16_t text_x = button_x + ((button_width - text_width) / 2);
            uint16_t text_y = button_y + ((button_height - 8) / 2); // Center 8px font vertically
            
            dwm_draw_text(text_x, text_y, button_text, 0xFFC0FFC0);
            }
            
            break;
            
        case DWM_EVENT_MOUSE: {
            uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
            uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
            
            // Close button hit check
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
