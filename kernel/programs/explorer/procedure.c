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

static void handle_explorer_mouse(WindowHandle handle, struct ExplorerWindowState* state, uint32_t wparam, int32_t lparam) {
    uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
    uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
    
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
            
            // --- CONTEXT MENU FOR AN ITEM ---
            if (lparam & DWM_STATE_MOUSE_BTN_RIGHT) {
                state->context_item_index = (int32_t)i;
                
                const char* item_menu_options[] = { "Open", "Copy", "Delete", "Properties" };
                dwm_summon_context_menu(handle, click_x, click_y, item_menu_options, 4);
                return;
            }
            
            // --- DOUBLE CLICK SELECTION ON AN ITEM ---
            if (lparam & DWM_STATE_MOUSE_DOUBLE_CLK) {
                if (state->fs_current == 0) {
                    if (state->items[i].icon_index == ICON_FOLDER) {
                        populate_state_from_knode(state, state->items[i].knode);
                    } 
                    else if (state->items[i].icon_index == ICON_STORAGE) {
                        uint32_t device_mount_address = knode_get_reference(state->items[i].knode, 0);
                        struct FSPartitionBlock partition;
                        fs_device_open(device_mount_address, &partition);
                        
                        populate_state_from_file_system(state, state->items[i].knode, partition.root_directory);
                    }
                } 
                else {
                    if (state->items[i].icon_index == ICON_FOLDER) {
                        populate_state_from_file_system(state, state->knode_current, state->items[i].fs_dir);
                    }
                }
                
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
            }
            break; 
        }
    }
    
    // --- CONTEXT MENU FOR OPEN WINDOW AREA ---
    // If it was a right click, but the loop above did not hit any files/folders
    if (!item_hit && (lparam & DWM_STATE_MOUSE_BTN_RIGHT)) {
        
        const char* window_menu_options[] = { "Refresh", "New Folder", "New File", "Paste", "Properties" };
        state->context_item_index = -1;
        dwm_summon_context_menu(handle, click_x, click_y, window_menu_options, 5);
        
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
    
    // Blank to a background color
    dwm_draw_rect_filled(EXPLORER_BG_X, EXPLORER_BG_Y, window_width, window_height, background);
    
    // Draw back button container components explicitly
    if (ui_button_back != NULL) {
        dwm_draw_rect_filled(BACK_BTN_CONTAINER_X, BACK_BTN_CONTAINER_Y, BACK_BTN_CONTAINER_W, BACK_BTN_CONTAINER_H, path_bg);
        dwm_draw_rect(BACK_BTN_BORDER_X, BACK_BTN_BORDER_Y, BACK_BTN_BORDER_W, BACK_BTN_BORDER_H, path_border);
        dwm_draw_sprite(BACK_BTN_SPRITE_X, BACK_BTN_SPRITE_Y, ui_button_back);
    }
    
    // Draw path input field background frame
    uint16_t path_field_width = window_width - PATH_FIELD_BG_X - 10;
    dwm_draw_rect_filled(PATH_FIELD_BG_X, PATH_FIELD_BG_Y, path_field_width, PATH_FIELD_BG_H, path_bg);
    dwm_draw_rect(PATH_FIELD_BORDER_X, PATH_FIELD_BORDER_Y, path_field_width + 2, PATH_FIELD_BORDER_H, path_border);
    
    // Color segmented path rendering mechanics
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
    
    // Navigation bar divider line
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
        
        // DEBUG - show item hit boxes
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
        switch (wparam) {
            
        case 0:
            dwm_summon_message_box("menu click", "test 0");
            break;
            
        case 1:
            dwm_summon_message_box("menu click", "test 1");
            break;
            
        case 2:
            dwm_summon_message_box("menu click", "item 2");
            break;
            
        case 3: // "Properties" clicked
            if (state->context_item_index != -1 && state->context_item_index < state->total_items) {
                struct Item* clicked_item = &state->items[state->context_item_index];
                
                dwm_summon_properties("Properties", clicked_item->name, clicked_item->path, clicked_item->icon_index);
            } else {
                
                dwm_summon_message_box("menu click", "item 3");
            };
            break;
            
        case 4:
            
            dwm_summon_properties("Properties", state->window_title, state->full_path, ICON_FOLDER);
            
            break;
            
        }
        return;
    }
}
