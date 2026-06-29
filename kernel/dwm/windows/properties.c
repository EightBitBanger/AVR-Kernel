#include <kernel/dwm/windows/properties.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/parser.h>

#include <kernel/vfs/vfs.h>
#include <kernel/events.h>

WindowHandle dwm_summon_properties(const char* title, const char* name, const char* file_path, uint16_t icon_index) {
    WindowClass wclass_props;
    uint16_t width  = 300;
    uint16_t height = 360;
    
    // Center the message box on the screen
    wclass_props.width  = width;
    wclass_props.height = height;
    wclass_props.x      = (display_get_width() - width) / 2;
    wclass_props.y      = (display_get_height() - height) / 2;
    
    // Assign boundaries limits
    wclass_props.max_width  = width;
    wclass_props.max_height = height;
    
    // Copy the title over to the class context
    strncpy(wclass_props.title, title, DWM_MAX_TITLE_LEN - 1);
    wclass_props.title[DWM_MAX_TITLE_LEN - 1] = '\0';
    
    struct WindowObject* msg_handle = dwm_allocate_window(
        wclass_props, 
        0, 
        (WindowProcedure)callback_properties_handler
    );
    
    // Instance metadata TAG
    char* instance = (char*)malloc(16);
    dwm_window_resource_add(msg_handle->id, "instance", instance);
    instance[0] = 0;
    
    // General - file metadata
    {
    size_t path_length = strnlen(file_path, DWM_MAX_PATH_LEN);
    size_t name_length = strnlen(name, DWM_MAX_PATH_LEN);
    
    char* target_path = (char*)malloc(DWM_MAX_PATH_LEN);
    char* target_name = (char*)malloc(DWM_MAX_PATH_LEN);
    char* target_type = (char*)malloc(DWM_MAX_PATH_LEN);
    char* target_size = (char*)malloc(DWM_MAX_PATH_LEN);
    
    uint16_t size = 0;
    
    File file = vfs_open(file_path, VFS_OPEN_READ);
    if (file != 0) {
        size = vfs_file_get_size(file);
        
        vfs_close(file);
    }
    
    uint16_t type_index = 0;
    if (vfs_is_directory(file_path)) {
        if (vfs_is_directory_mounted(file_path)) {
            type_index = 1;
            strncpy(target_type, "Storage", DWM_MAX_PATH_LEN);
        } else {
            type_index = 2;
            strncpy(target_type, "Folder", DWM_MAX_PATH_LEN);
        }
    } else {
        switch (icon_index) {
        case 1: type_index = 3; strncpy(target_type, "File", DWM_MAX_PATH_LEN); break;
        case 2: type_index = 4; strncpy(target_type, "Document", DWM_MAX_PATH_LEN); break;
        case 3: type_index = 5; strncpy(target_type, "System", DWM_MAX_PATH_LEN); break;
        }
    }
    
    switch (type_index) {
        
    default: // General file types
        itos(size, target_size);
        size_t length = strnlen(target_size, DWM_MAX_PATH_LEN);
        const char* bytes_str = " (bytes)";
        strncpy(&target_size[length], bytes_str, strnlen(bytes_str, DWM_MAX_PATH_LEN)+1);
        
        strncpy(target_path, file_path, path_length);
        target_path[path_length] = '\0';
        
        break;
        
    case 1: // Storage
        char* target_used = (char*)malloc(DWM_MAX_PATH_LEN);
        char* target_free = (char*)malloc(DWM_MAX_PATH_LEN);
        
        uint32_t used = vfs_device_get_used(file_path);
        itos(used, target_used);
        size_t len = strnlen(target_used, DWM_MAX_PATH_LEN);
        target_used[len] = '\0';
        
        target_free[0] = '\0';
        
        dwm_window_resource_add(msg_handle->id, "used", target_used);
        dwm_window_resource_add(msg_handle->id, "free", target_free);
        break;
        
    case 2: // Folder
        
        size_t item_count = vfs_directory_count(file_path);
        
        itos(item_count, target_size);
        size_t size_length = strnlen(target_size, DWM_MAX_PATH_LEN);
        
        break;
    }
    
    // File name
    strncpy(target_name, name, name_length);
    target_name[name_length] = '\0';
    
    strncpy(target_path, file_path, path_length);
    target_path[path_length] = '\0';
    
    dwm_window_resource_add(msg_handle->id, "path", target_path);
    dwm_window_resource_add(msg_handle->id, "name", target_name);
    dwm_window_resource_add(msg_handle->id, "type", target_type);
    dwm_window_resource_add(msg_handle->id, "size", target_size);
    }
    
    // Attributes
    {
    char* target_attrib = (char*)malloc(16);
    memset(target_attrib, ' ', 16);
    
    uint8_t permissions;
    vfs_get_permissions(file_path, &permissions);
    
    if (permissions & VFS_PERMISSION_EXECUTE) target_attrib[0] = 'x';
    if (permissions & VFS_PERMISSION_READ)    target_attrib[1] = 'r';
    if (permissions & VFS_PERMISSION_WRITE)   target_attrib[2] = 'w';
    
    dwm_window_resource_add(msg_handle->id, "perms", target_attrib);
    
    }
    
    dwm_set_focus(msg_handle);
    return msg_handle->id;
}


void callback_properties_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    char* instance = dwm_window_resource_get_by_name(handle, "instance");
    
    if (window == NULL) return;
    
    // ==========================================
    // LAYOUT & POSITIONING CONFIGURATION
    // ==========================================
    uint8_t current_tab = (instance != NULL) ? (uint8_t)instance[0] : 0;
    
    // Tab Layout (Adjusted gap/offsets for 3 tabs inside 300px width)
    uint16_t tab_width  = 76; 
    uint16_t tab_height = 24;
    uint16_t tab_gap    = 4; 
    
    uint16_t tab_general_x = 30; 
    uint16_t tab_general_y = 5;
    
    uint16_t tab_perm_x = tab_general_x + tab_width + tab_gap;
    uint16_t tab_perm_y = 5;

    uint16_t tab_security_x = tab_perm_x + tab_width + tab_gap;
    uint16_t tab_security_y = 5;
    
    // General Tab Layout Configurations
    uint16_t icon_x      = 24;
    uint16_t icon_y      = 54;
    
    // Permissions Tab Element Layout
    uint16_t box_size    = 12;
    uint16_t box_x       = 25;
    uint16_t label_x     = 45;
    uint16_t read_box_y  = 60;
    uint16_t write_box_y = 85;
    uint16_t exec_box_y  = 110; 
    
    // Close Button Layout
    uint16_t button_width  = 60;
    uint16_t button_height = 24;
    uint16_t button_x = window->w - (button_width + (button_width / 2));
    uint16_t button_y = ((window->h - button_height) - button_height) - 7;
    
    // Color Configurations
    uint32_t color_bg            = 0xFF08080F; 
    uint32_t color_divider       = 0xFF04C004; 
    uint32_t color_text_primary  = 0xFFFFFFFF; 
    uint32_t color_text_value    = 0xFF3FFF3F; 
    uint32_t color_text_inactive = 0xFF8888AA; 
    uint32_t color_border_active = 0xFF04C004; 
    uint32_t color_border_normal = 0xFF444466; 
    uint32_t color_fill_active   = 0xFF04C004; 
    uint32_t color_fill_button   = 0xFF1C1C2A; 
    
    switch (event) {
            
        case DWM_EVENT_KEYBOARD:
            if (wparam == 0x1B) 
                dwm_window_send_event(handle, DWM_EVENT_CLOSE);
            break;
            
        case DWM_EVENT_REDRAW:
            // Window background
            dwm_draw_rect_filled(0, 0, window->w, window->h, color_bg);
            dwm_draw_line(0, 35, window->w, 0, color_divider);
            
            // General tab
            if (current_tab == 0) {
                char* path_str = (char*)dwm_window_resource_get_by_name(handle, "path");
                dwm_draw_text(15, 110, "Path", color_text_primary);
                if (path_str != NULL) dwm_draw_text(15, 124, path_str, color_text_value);
                
                char* name_str = (char*)dwm_window_resource_get_by_name(handle, "name");
                if (name_str != NULL) dwm_draw_text(90, 78, name_str, color_text_value);
                
                uint16_t type_index = 0;
                char* type_str = (char*)dwm_window_resource_get_by_name(handle, "type");
                
                if (type_str != NULL) {
                    dwm_draw_text(90, 58, type_str, color_text_primary);
                    if (strncmp(type_str, "File",     5) == 0) {type_index = 1; dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_file"));}
                    else if (strncmp(type_str, "Folder",   7) == 0) {type_index = 2; dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_folder"));}
                    else if (strncmp(type_str, "Document", 9) == 0) {type_index = 3; dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_document"));}
                    else if (strncmp(type_str, "System",   7) == 0) {type_index = 4; dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_system"));}
                    else if (strncmp(type_str, "Storage",  8) == 0) {type_index = 5; dwm_draw_sprite(icon_x, icon_y, (struct Image*)dwm_resource_find("icon_storage"));}
                }
                
                if (type_index == 2) { 
                    char* size_str = (char*)dwm_window_resource_get_by_name(handle, "size");
                    dwm_draw_text(15, 150, "Items", color_text_primary);
                    if (size_str != NULL) dwm_draw_text(90, 150, size_str, color_text_value);
                } else if (type_index == 5) { 
                    char* used_str = (char*)dwm_window_resource_get_by_name(handle, "used");
                    char* free_str = (char*)dwm_window_resource_get_by_name(handle, "free");
                    
                    dwm_draw_text(15, 150, "Used", color_text_primary);
                    dwm_draw_text(15, 165, "Free", color_text_primary);
                    
                    if (used_str != NULL) dwm_draw_text(90, 150, used_str, color_text_value);
                    if (free_str != NULL) dwm_draw_text(90, 165, free_str, color_text_value);
                } else { 
                    char* size_str = (char*)dwm_window_resource_get_by_name(handle, "size");
                    dwm_draw_text(15, 150, "Size", color_text_primary);
                    if (size_str != NULL) dwm_draw_text(90, 150, size_str, color_text_value);
                }
            } 
            
            // Permissions tab
            else if (current_tab == 1) {
                char* attrib_str = (char*)dwm_window_resource_get_by_name(handle, "perms");
                
                if (attrib_str != NULL) {
                    int is_executable = 0;
                    int is_readable   = 0;
                    int is_writable   = 0;
                    
                    if (attrib_str[0] == 'x') is_executable = 1;
                    if (attrib_str[1] == 'r') is_readable = 1;
                    if (attrib_str[2] == 'w') is_writable = 1;
                    
                    dwm_draw_rect(box_x, read_box_y, box_size, box_size, color_border_normal);
                    if (is_readable) {
                        dwm_draw_rect_filled(box_x + 2, read_box_y + 2, box_size - 4, box_size - 4, color_fill_active);
                    }
                    dwm_draw_text(label_x, read_box_y + 2, "Readable", color_text_primary);
                    
                    dwm_draw_rect(box_x, write_box_y, box_size, box_size, color_border_normal);
                    if (is_writable) {
                        dwm_draw_rect_filled(box_x + 2, write_box_y + 2, box_size - 4, box_size - 4, color_fill_active);
                    }
                    dwm_draw_text(label_x, write_box_y + 2, "Writeable", color_text_primary);
                    
                    dwm_draw_rect(box_x, exec_box_y, box_size, box_size, color_border_normal);
                    if (is_executable) {
                        dwm_draw_rect_filled(box_x + 2, exec_box_y + 2, box_size - 4, box_size - 4, color_fill_active);
                    }
                    dwm_draw_text(label_x, exec_box_y + 2, "Executable", color_text_primary);
                }
            }
            
            // Security tab
            else if (current_tab == 2) {
                dwm_draw_text(15, 60, "Advanced Security Settings", color_text_primary);
                
                
                
                
            }
            
            // ------------------------------------------
            // TAB BUTTON RENDERING
            // ------------------------------------------
            
            // General Tab Button
            {
                int is_active = (current_tab == 0);
                uint32_t border_color = is_active ? color_border_active : color_border_normal; 
                uint32_t text_color   = is_active ? color_text_primary : color_text_inactive;
                
                dwm_draw_rect_filled(tab_general_x, tab_general_y, tab_width, tab_height, color_fill_button);
                dwm_draw_rect(tab_general_x, tab_general_y, tab_width, tab_height, border_color);
                
                const char* text = "General"; 
                dwm_draw_text(tab_general_x + ((tab_width - (strlen(text) * 6)) / 2), tab_general_y + 8, text, text_color);
            }
            
            // Permissions Tab Button
            {
                int is_active = (current_tab == 1);
                uint32_t border_color = is_active ? color_border_active : color_border_normal;
                uint32_t text_color   = is_active ? color_text_primary : color_text_inactive;
                
                dwm_draw_rect_filled(tab_perm_x, tab_perm_y, tab_width, tab_height, color_fill_button);
                dwm_draw_rect(tab_perm_x, tab_perm_y, tab_width, tab_height, border_color);
                
                const char* text = "Permissions";
                dwm_draw_text(tab_perm_x + ((tab_width - (strlen(text) * 6)) / 2), tab_perm_y + 8, text, text_color);
            }

            // Security Tab Button
            {
                int is_active = (current_tab == 2);
                uint32_t border_color = is_active ? color_border_active : color_border_normal;
                uint32_t text_color   = is_active ? color_text_primary : color_text_inactive;
                
                dwm_draw_rect_filled(tab_security_x, tab_security_y, tab_width, tab_height, color_fill_button);
                dwm_draw_rect(tab_security_x, tab_security_y, tab_width, tab_height, border_color);
                
                const char* text = "Security"; 
                dwm_draw_text(tab_security_x + ((tab_width - (strlen(text) * 6)) / 2), tab_security_y + 8, text, text_color);
            }
            
            // Close Button
            {
                dwm_draw_rect_filled(button_x, button_y, button_width, button_height, color_fill_button);
                dwm_draw_rect(button_x, button_y, button_width, button_height, color_border_normal);
                dwm_draw_text(button_x + ((button_width - 30) / 2), button_y + 8, "Close", color_text_value);
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
                // Security Tab Click Bounds
                else if (click_x >= tab_security_x && click_x < (tab_security_x + tab_width) && 
                         click_y >= tab_security_y && click_y < (tab_security_y + tab_height)) {
                    if (instance[0] != 2) {
                        instance[0] = 2;
                        state_changed = 1;
                    }
                }
                
                // Active checkbox processing inside permissions tab
                if (current_tab == 1) {
                    char* attrib_str = (char*)dwm_window_resource_get_by_name(handle, "perms");
                    char* path_str = (char*)dwm_window_resource_get_by_name(handle, "path");
                    
                    if (attrib_str && path_str) {
                        // Read checkbox
                        if (click_x >= box_x && click_x < (box_x + box_size) &&
                            click_y >= read_box_y && click_y < (read_box_y + box_size)) {
                            
                            char parent_path[DWM_MAX_PATH_LEN];
                            parse_get_parent_path(path_str, parent_path, DWM_MAX_PATH_LEN);
                            
                            uint8_t permissions;
                            vfs_get_permissions(parent_path, &permissions);
                            
                            if (permissions & VFS_PERMISSION_WRITE) {
                                attrib_str[1] = (attrib_str[1] == 'r') ? ' ' : 'r';
                                state_changed = 1;
                                
                                uint8_t perm = 0;
                                vfs_get_permissions(path_str, &perm);
                                
                                if (attrib_str[1] == 'r') {
                                    perm |= VFS_PERMISSION_READ;
                                } else {
                                    perm &= ~VFS_PERMISSION_READ;
                                }
                                vfs_set_permissions(path_str, perm);
                            }
                        }
                        // Write checkbox
                        else if (click_x >= box_x && click_x < (box_x + box_size) &&
                                 click_y >= write_box_y && click_y < (write_box_y + box_size)) {
                            
                            char parent_path[DWM_MAX_PATH_LEN];
                            parse_get_parent_path(path_str, parent_path, DWM_MAX_PATH_LEN);
                            
                            uint8_t permissions;
                            vfs_get_permissions(parent_path, &permissions);
                            
                            if (permissions & VFS_PERMISSION_WRITE) {
                                attrib_str[2] = (attrib_str[2] == 'w') ? ' ' : 'w';
                                state_changed = 1;
                                
                                uint8_t perm = 0;
                                vfs_get_permissions(path_str, &perm);
                                
                                if (attrib_str[2] == 'w') {
                                    perm |= VFS_PERMISSION_WRITE;
                                } else {
                                    perm &= ~VFS_PERMISSION_WRITE;
                                }
                                vfs_set_permissions(path_str, perm);
                            }
                        }
                        // Execute checkbox
                        else if (click_x >= box_x && click_x < (box_x + box_size) &&
                                 click_y >= exec_box_y && click_y < (exec_box_y + box_size)) {
                            
                            char parent_path[DWM_MAX_PATH_LEN];
                            parse_get_parent_path(path_str, parent_path, DWM_MAX_PATH_LEN);
                            
                            uint8_t permissions;
                            vfs_get_permissions(parent_path, &permissions);
                            
                            if (permissions & VFS_PERMISSION_WRITE) {
                                attrib_str[0] = (attrib_str[0] == 'x') ? ' ' : 'x';
                                state_changed = 1;
                                
                                uint8_t perm = 0;
                                vfs_get_permissions(path_str, &perm);
                                
                                if (attrib_str[0] == 'x') {
                                    perm |= VFS_PERMISSION_EXECUTE;
                                } else {
                                    perm &= ~VFS_PERMISSION_EXECUTE;
                                }
                                vfs_set_permissions(path_str, perm);
                            }
                        }
                    }
                }
                // Click processing inside security tab
                else if (current_tab == 2) {
                    // Implement handling for items inside your Security tab here if interactive
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
