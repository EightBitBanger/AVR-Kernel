#include <kernel/dwm/windows/message_box.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/parser.h>

#include <kernel/vfs/vfs.h>
#include <kernel/events.h>

WindowHandle dwm_summon_message_box(const char* title, const char* message) {
    WindowClass wclass_msgbox;
    uint16_t width  = 300;
    uint16_t height = 150;
    
    // Center the message box on the screen
    wclass_msgbox.width  = width;
    wclass_msgbox.height = height;
    wclass_msgbox.x      = (display_get_width() - width) / 2;
    wclass_msgbox.y      = (display_get_height() - height) / 2;
    
    // Assign boundaries limits
    wclass_msgbox.max_width  = width;
    wclass_msgbox.max_height = height;
    
    // Copy the title safely over to the class context
    strncpy(wclass_msgbox.title, title, DWM_MAX_TITLE_LEN - 1);
    wclass_msgbox.title[DWM_MAX_TITLE_LEN - 1] = '\0';
    
    struct WindowObject* msg_handle = dwm_allocate_window(
        wclass_msgbox, 
        0, 
        (WindowProcedure)callback_message_error_handler
    );
    
    // Add the message text as a window resource
    size_t message_length = strnlen(message, 1024);
    
    char* rc_message = (char*)malloc(message_length);
    strncpy(rc_message, message, message_length);
    rc_message[message_length] = '\0';
    
    dwm_window_resource_add(msg_handle->id, "text", rc_message);
    
    dwm_set_focus(msg_handle);
    
    return msg_handle->id;
}

void callback_message_box_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    uint16_t button_width  = 60;
    uint16_t button_height = 24;
    
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
            
            dwm_draw_text(text_x, text_y, button_text, 0xFF3FFF3F);
            
            uint16_t type_index = 0;
            char* type_str = (char*)dwm_window_resource_get_by_name(handle, "type");
            
            if (type_str != NULL) {
                dwm_draw_text(90, 58, type_str, 0xFFFFFFFF);
                     if (strncmp(type_str, "File",     5) == 0) {type_index = 1; dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_file"));}
                else if (strncmp(type_str, "Folder",   7) == 0) {type_index = 2; dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_folder"));}
                else if (strncmp(type_str, "Document", 9) == 0) {type_index = 3; dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_document"));}
                else if (strncmp(type_str, "System",   7) == 0) {type_index = 4; dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_system"));}
                else if (strncmp(type_str, "Storage",  8) == 0) {type_index = 5; dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_storage"));}
            }
            
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
            
        case DWM_EVENT_KEYBOARD:
            if (wparam == 0x1B) 
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            
            break;
            
        }
            
        default:
            break;
    }
}
