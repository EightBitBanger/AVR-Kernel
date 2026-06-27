#include <stdio.h>
#include <stdbool.h>

#include <kernel/kernel.h>
#include <kernel/dwm/dwm.h>
#include <kernel/events.h>
#include <kernel/util/string.h>

#include <kernel/programs/explorer/internal.h>

#include <kernel/arch/x86/malloc.h>
#include <kernel/knode.h>

static struct ExplorerWindowState* get_window_state(WindowHandle handle);

// Helper function to handle the actual VFS transaction and UI state updating
static void finalize_rename(WindowHandle handle, struct ExplorerWindowState* state) {
    if (state->edit_handle == 0 || state->context_item_index == -1) {
        return;
    }
    
    char new_name[128];
    memset(new_name, '\0', sizeof(new_name));
    
    // Extract string out of your updated text box component
    dwm_window_edit_get_text(handle, state->edit_handle, new_name, 127);
    
    struct Item* target_item = &state->items[state->context_item_index];
    
    // Prevent empty inputs or unnecessary VFS tasks if the string hasn't changed
    if (strlen(new_name) > 0 && strcmp(target_item->name, new_name) != 0) {
        
        vfs_rename(target_item->path, new_name);
    }
    
    if (state->fs_current != 0) {
        populate_state_from_file_system(state, state->knode_current, state->fs_current);
    } else {
        populate_state_from_knode(state, state->knode_current);
    }
    
    // Collapse and reset entry session states safely
    dwm_window_edit_visible(handle, state->edit_handle, false);
    state->context_item_index = -1; 
}

// Generates a unique folder or file path within the current directory context, handling index collisions gracefully
void generate_unique_name(struct ExplorerWindowState* state, char* out_path, const char* default_name) {
    memset(out_path, '\0', 128);
    
    // Copy the base path
    strncpy(out_path, state->full_path, 127);
    size_t base_len = strnlen(out_path, 127);
    
    size_t def_len = strnlen(default_name, 32);
    
    // Try the base name first (e.g. "/new_folder" or "/new_file")
    if (base_len + def_len < 127) {
        strncpy(&out_path[base_len], default_name, def_len);
    }
    
    // If it already exists, loop and append " (X)"
    if (vfs_exists(out_path)) {
        int counter = 1;
        
        while (1) {
            // Reset the buffer back to the base path
            memset(&out_path[base_len], '\0', 128 - base_len);
            size_t current_len = base_len;
            
            // Append base default name up to the starting paren
            strncpy(&out_path[current_len], default_name, def_len);
            current_len += def_len;
            
            const char* p1 = " (";
            strncpy(&out_path[current_len], p1, 2);
            current_len += 2;
            
            // Convert counter to string directly into the path buffer
            char num_buf[16];
            memset(num_buf, '\0', 16);
            itos(counter, num_buf);
            
            size_t num_len = strnlen(num_buf, 16);
            strncpy(&out_path[current_len], num_buf, num_len);
            current_len += num_len;
            
            // Append closing paren
            strncpy(&out_path[current_len], ")", 1);
            current_len += 1;
            
            // Unique path found! Break free.
            if (!vfs_exists(out_path)) {
                break;
            }
            
            counter++;
            
            // Safety guard to prevent infinite looping or buffer overruns
            if (current_len >= 120) break;
        }
    }
}

static void handle_explorer_mouse(WindowHandle handle, struct ExplorerWindowState* state, uint32_t wparam, int32_t lparam) {
    uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
    uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
    
    // Handle clicking away from rename field
    if (state->edit_handle != 0 && state->context_item_index != -1) {
        uint16_t max_cols = (state->win_width - NAV_X) / ITEM_WIDTH;
        if (max_cols == 0) max_cols = 1;
        
        uint16_t col = state->context_item_index % max_cols;
        uint16_t row = state->context_item_index / max_cols;
        
        uint16_t item_x = NAV_X + (col * ITEM_WIDTH);
        uint16_t item_y = NAV_Y + (row * ITEM_HEIGHT);
        
        // Check if click is outside the bounding box of the active renaming item grid cell
        if (click_x < item_x || click_x >= (item_x + ITEM_WIDTH) ||
            click_y < item_y || click_y >= (item_y + ITEM_HEIGHT)) {
            
            // Commit string modifications and invoke reloads
            finalize_rename(handle, state);
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
            
            // If right-clicked somewhere else, let it proceed to make a new menu. 
            // If it's a left click, eat the input here so it doesn't instantly click something else.
            if (!(lparam & DWM_STATE_MOUSE_BTN_RIGHT)) {
                return; 
            }
        }
    }
    
    // Back button hit detection
    if (ui_button_back != NULL) {
        uint16_t btn_x1 = BACK_BTN_SPRITE_X;
        uint16_t btn_y1 = BACK_BTN_SPRITE_Y;
        uint16_t btn_x2 = btn_x1 + ui_button_back->width;
        uint16_t btn_y2 = btn_y1 + ui_button_back->height;
        
        if (click_x >= btn_x1 && click_x < btn_x2 && click_y >= btn_y1 && click_y < btn_y2) {
            if (lparam & DWM_STATE_MOUSE_BTN_RIGHT) return; // Ignore right click on back button
            
            // Inside a mounted filesystem
            if (state->fs_current != 0) {
                uint32_t device_mount_address = knode_get_reference(state->knode_current, 0);
                struct FSPartitionBlock partition;
                fs_device_open(device_mount_address, &partition);
                
                if (state->fs_current == partition.root_directory) {
                    uint32_t parent_dir = knode_get_parent(state->knode_current);
                    populate_state_from_knode(state, parent_dir);
                } else {
                    uint32_t parent_fs_dir = fs_directory_get_parent(state->fs_current);
                    if (parent_fs_dir != FS_NULL && parent_fs_dir != 0) {
                        populate_state_from_file_system(state, state->knode_current, parent_fs_dir);
                    } else {
                        populate_state_from_file_system(state, state->knode_current, partition.root_directory);
                    }
                }
            } else {
                // Kernel memory directory node structure (ram disk)
                uint32_t parent_dir = knode_get_parent(state->knode_current);
                if (parent_dir != KNODE_NULL && parent_dir != 0 && state->knode_current != knode_get_root()) {
                    populate_state_from_knode(state, parent_dir);
                }
            }
            
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
            return;
        }
    }
    
    // Grid item collision loop (Handles both Double-Click selection and Right-Click item menus)
    uint16_t max_cols = (state->win_width - NAV_X) / ITEM_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    bool item_hit = false;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t tile_start_x = NAV_X + (col * ITEM_WIDTH);
        uint16_t tile_start_y = NAV_Y + (row * ITEM_HEIGHT);
        
        if (click_x >= tile_start_x && click_x < (tile_start_x + ITEM_WIDTH) &&
            click_y >= tile_start_y && click_y < (tile_start_y + ITEM_HEIGHT)) {
            
            item_hit = true;
            
            // Context menu for an item
            if (lparam & DWM_STATE_MOUSE_BTN_RIGHT) {
                state->context_item_index = (int32_t)i;
                
                const char* item_menu_options[] = { "Open", "Copy", "Rename", "Delete", "Properties" };
                dwm_summon_context_menu(handle, click_x, click_y, item_menu_options, 5);
                context_directive = 1;
                return;
            }
            
            // Double click selection on an item
            if (lparam & DWM_STATE_MOUSE_DOUBLE_CLK) {
                // Check if a file system is NOT currently in scope
                if (state->fs_current == 0) {
                    if (state->items[i].icon_index == ICON_FOLDER) {
                        populate_state_from_knode(state, state->items[i].knode);
                    } else if (state->items[i].icon_index == ICON_STORAGE) {
                        uint32_t device_mount_address = knode_get_reference(state->items[i].knode, 0);
                        struct FSPartitionBlock partition;
                        fs_device_open(device_mount_address, &partition);
                        
                        populate_state_from_file_system(state, state->items[i].knode, partition.root_directory);
                    } else { // Should be a file, open with notepad editor for now...
                        
                        kernel_event_send(KEVENT_EXECUTE, "notepad", state->full_path);
                    }
                } else if (state->items[i].icon_index == ICON_FOLDER) {
                    populate_state_from_file_system(state, state->knode_current, state->items[i].fs_dir);
                } else { // Should be a file, open with notepad editor for now...
                    
                    kernel_event_send(KEVENT_EXECUTE, "notepad", state->items[i].path);
                }
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
            }
            break; 
        }
    }
    
    // If it was a right click, but the loop above did not hit any files/folders
    if (!item_hit && (lparam & DWM_STATE_MOUSE_BTN_RIGHT)) {
        const char* window_menu_options[] = { "Refresh", "New Folder", "New File", "Paste", "Properties" };
        state->context_item_index = -1;
        dwm_summon_context_menu(handle, click_x, click_y, window_menu_options, 5);
        context_directive = 0;
        return;
    }
}

static void handle_explorer_resize(struct ExplorerWindowState* state, uint32_t wparam) {
    state->win_width = (uint16_t)(wparam & 0xFFFF);
    state->win_height = (uint16_t)((wparam >> 16) & 0xFFFF);
}

static void handle_explorer_redraw(WindowHandle handle, struct ExplorerWindowState* state) {
    uint16_t window_width = dwm_window_get_width(handle);
    uint16_t window_height = dwm_window_get_height(handle);
    
    dwm_draw_rect_filled(EXPLORER_BG_X, EXPLORER_BG_Y, window_width, window_height, background);
    
    if (ui_button_back != NULL) {
        dwm_draw_rect_filled(BACK_BTN_CONTAINER_X, BACK_BTN_CONTAINER_Y, BACK_BTN_CONTAINER_W, BACK_BTN_CONTAINER_H, path_bg);
        dwm_draw_rect(BACK_BTN_BORDER_X, BACK_BTN_BORDER_Y, BACK_BTN_BORDER_W, BACK_BTN_BORDER_H, path_border);
        dwm_draw_sprite(BACK_BTN_SPRITE_X, BACK_BTN_SPRITE_Y, ui_button_back);
    }
    
    uint16_t path_field_width = window_width - PATH_FIELD_BG_X - 10;
    dwm_draw_rect_filled(PATH_FIELD_BG_X, PATH_FIELD_BG_Y, path_field_width, PATH_FIELD_BG_H, path_bg);
    dwm_draw_rect(PATH_FIELD_BORDER_X, PATH_FIELD_BORDER_Y, path_field_width + 2, PATH_FIELD_BORDER_H, path_border);
    
    if (state->fs_current == 0 || state->knode_path_len >= strlen(state->path)) {
        dwm_draw_text(PATH_TEXT_X, PATH_TEXT_Y, state->path, text_knode);
    } else {
        char base_buffer[MAX_PATH_LEN];
        memset(base_buffer, 0, MAX_PATH_LEN);
        strncpy(base_buffer, state->path, state->knode_path_len);
        
        dwm_draw_text(PATH_TEXT_X, PATH_TEXT_Y, base_buffer, text_knode);
        int16_t mount_offset_x = PATH_TEXT_X + (state->knode_path_len * PATH_FONT_CHAR_WIDTH);
        dwm_draw_text(mount_offset_x, PATH_TEXT_Y, state->path + state->knode_path_len, text_mount);
    }
    
    dwm_draw_line(NAV_DIVIDER_X, NAV_DIVIDER_Y, window_width, NAV_DIVIDER_H, navbar_div);
    
    uint16_t max_cols = (state->win_width - NAV_X) / ITEM_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t sp_x = NAV_X + (col * ITEM_WIDTH);
        uint16_t sp_y = (NAV_Y + (row * ITEM_HEIGHT)) + NAV_Y_OFF;
        
        struct Image* current_icon;
        switch (state->items[i].icon_index) {
            case ICON_FOLDER:    current_icon = icon_folder; break;
            case ICON_FILE:      current_icon = icon_file; break;
            case ICON_DOCUMENT:  current_icon = icon_document; break;
            case ICON_SYSTEM:    current_icon = icon_system; break;
            case ICON_STORAGE:   current_icon = icon_storage; break;
            default: current_icon = 0; break;
        }
        
        if (current_icon != NULL) {
            uint16_t icon_offset_x = (ITEM_WIDTH - current_icon->width) / 2; 
            dwm_draw_sprite(sp_x + icon_offset_x, sp_y, current_icon);
        }
        
        size_t length = strlen(state->items[i].name);
        uint16_t string_width = length * ITEM_FONT_CHAR_WIDTH;
        int16_t text_start_x = sp_x + ((ITEM_WIDTH - string_width) / 2);
        dwm_draw_text(text_start_x, sp_y + ITEM_TEXT_HEIGHT_OFF, state->items[i].name, item_text);
        
#ifdef _DEBUG_DRAW_ENABLE_
        uint16_t tile_start_x = NAV_X + (col * ITEM_WIDTH);
        uint16_t tile_start_y = NAV_Y + (row * ITEM_HEIGHT);
        dwm_draw_rect(tile_start_x, tile_start_y, ITEM_WIDTH, ITEM_HEIGHT, 0xFF808080);
#endif
    }
}

struct ExplorerWindowState* get_window_state(WindowHandle handle) {
    struct ExplorerWindowState* current = window_list_head;
    while (current != NULL) {
        if (current->handle == handle) return current;
        current = current->next;
    }
    return NULL;
}

void callback_handler_explorer(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct ExplorerWindowState* state = get_window_state(handle);
    if (!state) return;
    
    switch (event) {
    case DWM_EVENT_MOUSE:
        handle_explorer_mouse(handle, state, wparam, lparam);
        break;
    
    case DWM_EVENT_KEYBOARD:
        if (state->edit_handle != 0) {
            char ch[] = {wparam & 0xFF, '\0'};
            
            // Handle Return / Enter Key
            if (ch[0] == 0x02) {
                finalize_rename(handle, state);
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                break;
            }
            
            // Recompute the cell's base item_x so we know our centering origin
            uint16_t max_cols = (state->win_width - NAV_X) / ITEM_WIDTH;
            if (max_cols == 0) max_cols = 1;
            uint16_t col = state->context_item_index % max_cols;
            uint16_t row = state->context_item_index / max_cols;
            uint16_t item_x = NAV_X + (col * ITEM_WIDTH);
            uint16_t item_y = NAV_Y + (row * ITEM_HEIGHT) + 1;
            
            // Handle Backspace
            if (ch[0] == 0x01) {
                dwm_window_edit_backspace(handle, state->edit_handle);
                
                size_t current_len = dwm_window_edit_get_len(handle, state->edit_handle);
                uint16_t new_width = current_len * ITEM_FONT_CHAR_WIDTH;
                
                if (new_width < 30) new_width = 30; 
                state->edit_width = new_width;
                
                int16_t edit_x = item_x + ((ITEM_WIDTH - state->edit_width) / 2);
                
                dwm_window_edit_set_width(handle, state->edit_handle, state->edit_width);
                dwm_window_edit_set_pos(handle, state->edit_handle, edit_x, item_y + ITEM_TEXT_HEIGHT_OFF);
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                break;
            }
            
            // Handle Character Insertion
            dwm_window_edit_insert(handle, state->edit_handle, ch);
            
            size_t current_len = dwm_window_edit_get_len(handle, state->edit_handle);
            uint16_t new_width = current_len * ITEM_FONT_CHAR_WIDTH;
            
            if (new_width > MAX_RENAME_WIDTH) {
                new_width = MAX_RENAME_WIDTH;
            } else if (new_width < 30) {
                new_width = 30;
            }
            
            state->edit_width = new_width;
            
            int16_t edit_x = item_x + ((ITEM_WIDTH - state->edit_width) / 2);
            
            dwm_window_edit_set_width(handle, state->edit_handle, state->edit_width);
            dwm_window_edit_set_pos(handle, state->edit_handle, edit_x, item_y + ITEM_TEXT_HEIGHT_OFF);
            
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
        break;
        
    case DWM_EVENT_RESIZE:
        handle_explorer_resize(state, wparam);
        break;
        
    case DWM_EVENT_REDRAW:
        handle_explorer_redraw(handle, state);
        break;
        
    case DWM_EVENT_DESTROY:
        free_window_state(handle);
        return;
        
    case DWM_EVENT_CONTEXT_MENU:
        switch (context_directive) {
        case 0:  // Open hit
            switch (wparam) {
            case 0:
                dwm_summon_message_box("menu click", "refresh");
                break;
                
            case 1: { // New Folder
                char new_folder_path[128];
                generate_unique_name(state, new_folder_path, "/new_folder");
                vfs_mkdir(new_folder_path);
                
                if (state->fs_current != 0) {
                    populate_state_from_file_system(state, state->knode_current, state->fs_current);
                } else {
                    populate_state_from_knode(state, state->knode_current);
                }
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                break;
            }
                
            case 2: { // New File
                char new_file_path[128];
                generate_unique_name(state, new_file_path, "/new_file");
                vfs_mkfile(new_file_path, 0);
                
                if (state->fs_current != 0) {
                    populate_state_from_file_system(state, state->knode_current, state->fs_current);
                } else {
                    populate_state_from_knode(state, state->knode_current);
                }
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                break;
            }
            case 3:
                dwm_summon_message_box("menu click", "paste");
                state->context_item_index = -1;
                break;
            case 4:
                dwm_summon_properties("Properties", state->window_title, state->full_path, ICON_FOLDER);
                state->context_item_index = -1;
                break;
            }
            break;
            
        case 1:  // Item hit
            switch (wparam) {
            case 0:
                dwm_summon_message_box("menu click", "open");
                state->context_item_index = -1;
                break;
            case 1:
                dwm_summon_message_box("menu click", "copy");
                state->context_item_index = -1;
                break;
            case 2: // Rename file/folder
                if (state->edit_handle != 0 && state->context_item_index != -1) {
                    uint16_t max_cols = (state->win_width - NAV_X) / ITEM_WIDTH;
                    if (max_cols == 1) max_cols = 1;
                    struct Item* clicked_item = &state->items[state->context_item_index];
    
                    uint16_t col = state->context_item_index % max_cols;
                    uint16_t row = state->context_item_index / max_cols;
                    
                    uint16_t item_x = NAV_X + (col * ITEM_WIDTH);
                    uint16_t item_y = NAV_Y + (row * ITEM_HEIGHT) + 1;
                    
                    size_t name_len = strlen(clicked_item->name);
                    uint16_t initial_width = name_len * ITEM_FONT_CHAR_WIDTH;
                    
                    if (initial_width < 30) initial_width = 30;
                    if (initial_width > MAX_RENAME_WIDTH) initial_width = MAX_RENAME_WIDTH;
                    
                    state->edit_width = initial_width;
                    dwm_window_edit_set_width(handle, state->edit_handle, state->edit_width);
                    
                    int16_t edit_x = item_x + ((ITEM_WIDTH - state->edit_width) / 2);
                    
                    dwm_window_edit_text(handle, state->edit_handle, clicked_item->name);
                    dwm_window_edit_set_pos(handle, state->edit_handle, edit_x, item_y + ITEM_TEXT_HEIGHT_OFF);
                    dwm_window_edit_visible(handle, state->edit_handle, true);
                    
                    clicked_item->name[0] = '\0';
                    
                    dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                }
                break;
            case 3: {
                struct Item* clicked_item = &state->items[state->context_item_index];
                
                //dwm_summon_dialog_delete("Deletion request", clicked_item->path);
                
                vfs_remove(clicked_item->path);
                
                if (state->fs_current != 0) {
                    populate_state_from_file_system(state, state->knode_current, state->fs_current);
                } else {
                    populate_state_from_knode(state, state->knode_current);
                }
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                
                state->context_item_index = -1;
                break;
                }
            case 4: {
                struct Item* clicked_item = &state->items[state->context_item_index];
                
                dwm_summon_properties("Properties", clicked_item->name, clicked_item->path, clicked_item->icon_index);
                
                state->context_item_index = -1;
                break;
                }
            }
            break;
        }
        
        return;
    }
}
