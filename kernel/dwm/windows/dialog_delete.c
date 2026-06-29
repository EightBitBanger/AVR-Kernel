#include <kernel/dwm/windows/dialog_delete.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/parser.h>

#include <kernel/vfs/vfs.h>
#include <kernel/events.h>

WindowHandle dwm_summon_dialog_delete(const char* title, const char* file_path, WindowHandle parent, uint16_t icon_index) {
    WindowClass wclass_msgbox;
    uint16_t width  = 400;
    uint16_t height = 180;
    
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
        (WindowProcedure)callback_deletion_dialog_handler
    );
    
    // Add the message text as a window resource
    size_t path_length = strnlen(file_path, 128);
    
    char* target_path = (char*)malloc(DWM_MAX_PATH_LEN);
    char* target_type = (char*)malloc(DWM_MAX_PATH_LEN);
    
    strncpy(target_path, file_path, path_length);
    target_path[path_length] = '\0';
    
    if (vfs_is_directory(file_path)) {
        if (vfs_is_directory_mounted(file_path)) {
            strncpy(target_type, "Storage", DWM_MAX_PATH_LEN);
        } else {
            strncpy(target_type, "Folder", DWM_MAX_PATH_LEN);
        }
    } else {
        switch (icon_index) {
        case 1: strncpy(target_type, "File", DWM_MAX_PATH_LEN); break;
        case 2: strncpy(target_type, "Document", DWM_MAX_PATH_LEN); break;
        case 3: strncpy(target_type, "System", DWM_MAX_PATH_LEN); break;
        }
    }
    
    dwm_window_resource_add(msg_handle->id, "path", target_path);
    dwm_window_resource_add(msg_handle->id, "type", target_type);
    
    dwm_set_focus(msg_handle);
    return msg_handle->id;
}

void callback_deletion_dialog_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    // ==========================================
    // Layout & positioning
    
    // Button dimensions and spacing
    uint16_t button_width       = 60;
    uint16_t button_height      = 24;
    uint16_t button_gap         = 20;
    uint16_t button_padding     = 14; // Bottom padding for buttons
    
    // Icon layout
    uint16_t icon_x             = 20;
    uint16_t icon_y             = 24;
    
    // Text layout
    uint16_t text_x             = 85; // X position for all texts
    uint16_t prompt_y           = 50; // Y position for file path
    uint16_t subtext_y          = 26; // Y position for "Are you sure..." prompt
    uint32_t file_path_color    = 0xFFC00404;
    
    uint16_t total_buttons_width = (button_width * 2) + button_gap;
    uint16_t yes_button_x = (window->w - total_buttons_width) / 2;
    uint16_t no_button_x  = yes_button_x + button_width + button_gap;
    uint16_t button_y     = window->h - button_height - button_height - button_padding;
    
    switch (event) {
        case DWM_EVENT_REDRAW: {
            // Window background
            dwm_draw_rect_filled(0, 0, window->w, window->h, 0xFF100101);
            
            // Fetch resources
            char* path_resource = (char*)dwm_window_resource_get_by_name(handle, "path");
            char* type_str      = (char*)dwm_window_resource_get_by_name(handle, "type");
            
            // Draw the appropriate icon if the type resource exists
            uint16_t type_index = 0;
            if (type_str != NULL) {
                if (strncmp(type_str, "File", 5) == 0) {
                    dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_file")); type_index = 1;
                } else if (strncmp(type_str, "Folder", 7) == 0) {
                    dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_folder")); type_index = 2;
                } else if (strncmp(type_str, "Document", 9) == 0) {
                    dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_document")); type_index = 3;
                } else if (strncmp(type_str, "System", 7) == 0) {
                    dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_system")); type_index = 4;
                } else if (strncmp(type_str, "Storage", 8) == 0) {
                    dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_storage")); type_index = 5;
                }
            }
            
            // Render confirmation prompt text
            if (path_resource != NULL) {
                char format_buffer[DWM_MAX_PATH_LEN];
                strncpy(format_buffer, path_resource, DWM_MAX_PATH_LEN);
                
                char prompt[DWM_MAX_PATH_LEN] = "Are you sure you want to delete this %?";
                str_replace(prompt, "%", type_str, DWM_MAX_PATH_LEN);
                
                // Lowercase the string except the first character
                str_tolower(&prompt[1]);
                
                dwm_draw_text(text_x, subtext_y, prompt, window->title_text_color);
                dwm_draw_text(text_x, prompt_y, format_buffer, file_path_color);
            }
            
            // Draw "Yes" button
            dwm_draw_rect_filled(yes_button_x, button_y, button_width, button_height, 0xFF1C1C2A);
            dwm_draw_rect(yes_button_x, button_y, button_width, button_height, 0xFF444466);
            
            const char* yes_text = "Yes";
            uint16_t yes_text_x = yes_button_x + ((button_width - (strlen(yes_text) * 6)) / 2);
            uint16_t yes_text_y = button_y + ((button_height - 8) / 2);
            dwm_draw_text(yes_text_x, yes_text_y, yes_text, 0xFF3FFF3F); 
            
            // Draw "No" button
            dwm_draw_rect_filled(no_button_x, button_y, button_width, button_height, 0xFF1C1C2A);
            dwm_draw_rect(no_button_x, button_y, button_width, button_height, 0xFF444466);
            
            const char* no_text = "No";
            uint16_t no_text_x = no_button_x + ((button_width - (strlen(no_text) * 6)) / 2);
            uint16_t no_text_y = button_y + ((button_height - 8) / 2);
            dwm_draw_text(no_text_x, no_text_y, no_text, 0xFF3FFF3F); 
            
            break;
        }
            
        case DWM_EVENT_MOUSE: {
            uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
            uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
            
            // Hit-test "Yes" Button using the exact same configured variables
            if (click_x >= yes_button_x && click_x < (yes_button_x + button_width) && 
                click_y >= button_y && click_y < (button_y + button_height)) {
                
                char* path_resource = (char*)dwm_window_resource_get_by_name(handle, "path");
                if (path_resource != NULL) {
                    if (!vfs_remove(path_resource)) {
                        dwm_summon_message_error("Deletion error", "Access denied", "", "image_error");
                    }
                }
                
                kernel_event_send(KEVENT_DWM_REFRESH, "", "");
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            }
            // Hit-test "No" Button
            else if (click_x >= no_button_x && click_x < (no_button_x + button_width) && 
                     click_y >= button_y && click_y < (button_y + button_height)) {
                
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            }
            break;
        }
            
        case DWM_EVENT_KEYBOARD:
            if (wparam == 0x1B) {
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            }
            break;
            
        default:
            break;
    }
}
