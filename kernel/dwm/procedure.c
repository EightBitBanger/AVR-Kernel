#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>

#include <kernel/vfs/vfs.h>
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
            
            dwm_draw_text(text_x, text_y, button_text, 0xFF3FFF3F);
            
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
    char* instance = dwm_window_resource_get_by_name(handle, "instance");
    
    if (window == NULL) return;
    
    // ==========================================
    // LAYOUT & POSITIONING CONFIGURATION
    // ==========================================
    uint8_t current_tab = (instance != NULL) ? (uint8_t)instance[0] : 0;
    
    // Tab Layout
    uint16_t tab_width  = 80; 
    uint16_t tab_height = 24;
    uint16_t tab_gap    = 6; 
    
    uint16_t tab_general_x = 30; 
    uint16_t tab_general_y = 5;
    
    uint16_t tab_perm_x = tab_general_x + tab_width + tab_gap;
    uint16_t tab_perm_y = 5;
    
    // Permissions Tab Element Layout
    uint16_t box_size    = 12;
    uint16_t box_x       = 25;
    uint16_t label_x     = 45;
    uint16_t read_box_y  = 60;
    uint16_t write_box_y = 85;
    uint16_t exec_box_y  = 110; // Added positioning for Execute checkbox
    
    // Close Button Layout
    uint16_t button_width  = 60;
    uint16_t button_height = 24;
    uint16_t button_x = window->w - (button_width + (button_width / 2));
    uint16_t button_y = ((window->h - button_height) - button_height) - 7;
    
    switch (event) {
        case DWM_EVENT_REDRAW:
            // Window background
            dwm_draw_rect_filled(0, 0, window->w, window->h, 0xFF08080F);
            dwm_draw_line(0, 35, window->w, 0, 0xFF04C004);
            
            // ==========================================
            // CONDITIONAL CONTENT RENDERING
            // ==========================================
            if (current_tab == 0) { // General
                char* path_str = (char*)dwm_window_resource_get_by_name(handle, "path");
                dwm_draw_text(15, 110, "Path", 0xFFFFFFFF);
                if (path_str != NULL) dwm_draw_text(15, 124, path_str, 0xFF3FFF3F);
                
                char* name_str = (char*)dwm_window_resource_get_by_name(handle, "name");
                if (name_str != NULL) dwm_draw_text(90, 78, name_str, 0xFF3FFF3F);
                
                char* type_str = (char*)dwm_window_resource_get_by_name(handle, "type");
                if (type_str != NULL) {
                    dwm_draw_text(90, 58, type_str, 0xFFFFFFFF);
                    if (strncmp(type_str, "File", 5) == 0) {dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_file"));}
                    else if (strncmp(type_str, "Folder", 7) == 0) {dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_folder"));}
                    else if (strncmp(type_str, "Document", 9) == 0) {dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_document"));}
                    else if (strncmp(type_str, "System", 7) == 0) {dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_system"));}
                    else if (strncmp(type_str, "Storage", 8) == 0) {dwm_draw_sprite(20, 54, (struct Image*)dwm_resource_find("icon_storage"));}
                }
            } 
            else if (current_tab == 1) { // Permissions
                char* attrib_str = (char*)dwm_window_resource_get_by_name(handle, "perms");
                
                if (attrib_str != NULL) {
                    int is_executable = 0;
                    int is_readable   = 0;
                    int is_writable   = 0;
                    
                    if (attrib_str[0] == 'x') is_executable = 1;
                    if (attrib_str[1] == 'r') is_readable = 1;
                    if (attrib_str[2] == 'w') is_writable = 1;
                    
                    // Read Checkbox
                    dwm_draw_rect(box_x, read_box_y, box_size, box_size, 0xFF444466);
                    if (is_readable) {
                        dwm_draw_rect_filled(box_x + 2, read_box_y + 2, box_size - 4, box_size - 4, 0xFF04C004);
                    }
                    dwm_draw_text(label_x, read_box_y + 2, "Readable", 0xFFFFFFFF);
                    
                    // Write Checkbox
                    dwm_draw_rect(box_x, write_box_y, box_size, box_size, 0xFF444466);
                    if (is_writable) {
                        dwm_draw_rect_filled(box_x + 2, write_box_y + 2, box_size - 4, box_size - 4, 0xFF04C004);
                    }
                    dwm_draw_text(label_x, write_box_y + 2, "Writeable", 0xFFFFFFFF);

                    // Execute Checkbox (Added UI Rendering)
                    dwm_draw_rect(box_x, exec_box_y, box_size, box_size, 0xFF444466);
                    if (is_executable) {
                        dwm_draw_rect_filled(box_x + 2, exec_box_y + 2, box_size - 4, box_size - 4, 0xFF04C004);
                    }
                    dwm_draw_text(label_x, exec_box_y + 2, "Executable", 0xFFFFFFFF);
                }
            }
            
            // ==========================================
            // TAB BUTTON RENDERING
            // ==========================================
            // General Tab Button
            {
                int is_active = (current_tab == 0);
                uint32_t border_color = is_active ? 0xFF04C004 : 0xFF444466; 
                uint32_t text_color   = is_active ? 0xFFFFFFFF : 0xFF8888AA;
                
                dwm_draw_rect_filled(tab_general_x, tab_general_y, tab_width, tab_height, 0xFF1C1C2A);
                dwm_draw_rect(tab_general_x, tab_general_y, tab_width, tab_height, border_color);
                
                const char* text = "General"; 
                dwm_draw_text(tab_general_x + ((tab_width - (strlen(text) * 6)) / 2), tab_general_y + 8, text, text_color);
            }
            
            // Permissions Tab Button
            {
                int is_active = (current_tab == 1);
                uint32_t border_color = is_active ? 0xFF04C004 : 0xFF444466;
                uint32_t text_color   = is_active ? 0xFFFFFFFF : 0xFF8888AA;
                
                dwm_draw_rect_filled(tab_perm_x, tab_perm_y, tab_width, tab_height, 0xFF1C1C2A);
                dwm_draw_rect(tab_perm_x, tab_perm_y, tab_width, tab_height, border_color);
                
                const char* text = "Permissions"; 
                dwm_draw_text(tab_perm_x + ((tab_width - (strlen(text) * 6)) / 2), tab_perm_y + 8, text, text_color);
            }
            
            // Close Button
            {
                dwm_draw_rect_filled(button_x, button_y, button_width, button_height, 0xFF1C1C2A);
                dwm_draw_rect(button_x, button_y, button_width, button_height, 0xFF444466);
                dwm_draw_text(button_x + ((button_width - 30) / 2), button_y + 8, "Close", 0xFF3FFF3F);
            }
            break;
            
        case DWM_EVENT_MOUSE: {
            if (lparam & DWM_STATE_MOUSE_BTN_RIGHT) 
                break;
            
            uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
            uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
            int state_changed = 0;
            
            if (instance != NULL) {
                // General Tab Click Bounds
                if (click_x >= tab_general_x && click_x < (tab_general_x + tab_width) && 
                    click_y >= tab_general_y && click_y < (tab_general_y + tab_height)) {
                    if (instance[0] != 0) {
                        instance[0] = 0;
                        state_changed = 1;
                    }
                }
                // Permissions Tab Click Bounds
                else if (click_x >= tab_perm_x && click_x < (tab_perm_x + tab_width) && 
                         click_y >= tab_perm_y && click_y < (tab_perm_y + tab_height)) {
                    if (instance[0] != 1) {
                        instance[0] = 1;
                        state_changed = 1;
                    }
                }
                
                // Active Checkbox Processing inside Permissions Tab
                if (current_tab == 1) {
                    char* attrib_str = (char*)dwm_window_resource_get_by_name(handle, "perms");
                    char* path_str = (char*)dwm_window_resource_get_by_name(handle, "path");
                    
                    if (attrib_str && path_str) {
                        
                        // Read checkbox bounds check
                        if (click_x >= box_x && click_x < (box_x + box_size) &&
                            click_y >= read_box_y && click_y < (read_box_y + box_size)) {
                            
                            attrib_str[1] = (attrib_str[1] == 'r') ? ' ' : 'r';
                            state_changed = 1;
                            
                            uint32_t address = resolve_path_to_address(path_str);
                            
                            if (fs_file_check(address) || fs_check_directory_valid(address)) {
                                uint8_t perm;
                                fs_file_get_permissions(address, &perm);
                                
                                if (attrib_str[1] == 'r') {
                                    perm |= VFS_PERMISSION_READ;
                                } else {
                                    perm &= ~VFS_PERMISSION_READ;
                                }
                                
                                fs_file_set_permissions(address, perm);
                            }
                        }
                        
                        // Write checkbox bounds check
                        else if (click_x >= box_x && click_x < (box_x + box_size) &&
                                click_y >= write_box_y && click_y < (write_box_y + box_size)) {
                            
                            attrib_str[2] = (attrib_str[2] == 'w') ? ' ' : 'w';
                            state_changed = 1;
                            
                            uint32_t address = resolve_path_to_address(path_str);
                            
                            if (fs_file_check(address) || fs_check_directory_valid(address)) {
                                uint8_t perm;
                                fs_file_get_permissions(address, &perm);
                                
                                if (attrib_str[2] == 'w') {
                                    perm |= VFS_PERMISSION_WRITE;
                                } else {
                                    perm &= ~VFS_PERMISSION_WRITE;
                                }
                                
                                fs_file_set_permissions(address, perm);
                            }
                        }
                        
                        // Execute checkbox bounds check
                        else if (click_x >= box_x && click_x < (box_x + box_size) &&
                                click_y >= exec_box_y && click_y < (exec_box_y + box_size)) {
                            
                            attrib_str[0] = (attrib_str[0] == 'x') ? ' ' : 'x';
                            state_changed = 1;
                            
                            uint32_t address = resolve_path_to_address(path_str);
                            
                            if (fs_file_check(address) || fs_check_directory_valid(address)) {
                                uint8_t perm;
                                fs_file_get_permissions(address, &perm);
                                
                                if (attrib_str[0] == 'x') {
                                    perm |= VFS_PERMISSION_EXECUTE;
                                } else {
                                    perm &= ~VFS_PERMISSION_EXECUTE;
                                }
                                
                                fs_file_set_permissions(address, perm);
                            }
                        }
                    }
                }
            }
            
            // Close Button Click Bounds
            if (click_x >= button_x && click_x < (button_x + button_width) && 
                click_y >= button_y && click_y < (button_y + button_height)) {
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            }
            
            if (state_changed) {
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
            }
            break;
        }
        default: 
            break;
    }
}
