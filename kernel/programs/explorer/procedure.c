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
    if (lparam & EVENT_STATE_MOUSE_BTN_RIGHT) 
        return;
    
    uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
    uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
    
    // Back button bounding box detection
    if (ui_button_back != NULL) {
        uint16_t btn_x1 = 7;
        uint16_t btn_y1 = 3;
        uint16_t btn_x2 = btn_x1 + ui_button_back->width;
        uint16_t btn_y2 = btn_y1 + ui_button_back->height;
        
        if (click_x >= btn_x1 && click_x < btn_x2 && click_y >= btn_y1 && click_y < btn_y2) {
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
                uint32_t parent_dir = knode_get_parent(state->knode_current);
                if (parent_dir != KNODE_NULL && parent_dir != 0 && state->knode_current != knode_get_root()) {
                    populate_state_from_knode(state, parent_dir);
                }
            }
            
            dwm_window_send_event(handle, EVENT_REDRAW);
            return;
        }
    }
    
    // Window icon double click
    if (!(lparam & EVENT_STATE_MOUSE_DOUBLE_CLK)) 
        return;
    
    uint16_t max_cols = (state->win_width - TILE_BASE_X) / TILE_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t tile_start_x = TILE_BASE_X + (col * TILE_WIDTH);
        uint16_t tile_start_y = TILE_BASE_Y + (row * TILE_HEIGHT);
        
        if (click_x >= tile_start_x && click_x < (tile_start_x + TILE_WIDTH) &&
            click_y >= tile_start_y && click_y < (tile_start_y + TILE_HEIGHT)) {
            
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
            
            dwm_window_send_event(handle, EVENT_REDRAW);
            break;
        }
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
    dwm_draw_rect_filled(0, 0, window_width, window_height, background);
    
    int16_t path_text_x = 12;
    if (ui_button_back != NULL) {
        path_text_x = 7 + ui_button_back->width + 12;
    }
    
    if (ui_button_back != NULL) {
        dwm_draw_rect_filled(5, 4, path_text_x - 15, 16, path_bg);
        dwm_draw_rect(4, 3, path_text_x - 14, 18, path_border);
        dwm_draw_sprite(7, 1, ui_button_back);
    }
    
    dwm_draw_rect_filled(path_text_x - 5, 4, window_width - path_text_x, 16, path_bg);
    dwm_draw_rect(path_text_x - 4, 3, window_width - path_text_x, 18, path_border);
    
    // Color segmented path rendering mechanics
    if (state->fs_current == 0 || state->knode_path_len >= strlen(state->path)) {
        // Standard View: Render everything in classic Virtual KNode green
        dwm_draw_text(path_text_x, 8, state->path, text_knode);
    } else {
        // Split View: Read slice limits to split string processing
        char base_buffer[MAX_PATH_LEN];
        memset(base_buffer, 0, MAX_PATH_LEN);
        strncpy(base_buffer, state->path, state->knode_path_len);
        
        // Draw Virtual KNode sequence in Green
        dwm_draw_text(path_text_x, 8, base_buffer, text_knode);
        
        // Calculate pixel offset (assuming standard 6px per char font width)
        int16_t mount_offset_x = path_text_x + (state->knode_path_len * 6);
        
        // Draw the rest of the file system path
        dwm_draw_text(mount_offset_x, 8, state->path + state->knode_path_len, text_mount);
    }
    
    // Navigation bar divider
    dwm_draw_line(0, 25, window_width, 0, navbar_div);
    
    uint16_t max_cols = (state->win_width - TILE_BASE_X) / TILE_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t sp_x = TILE_BASE_X + (col * TILE_WIDTH);
        uint16_t sp_y = TILE_BASE_Y + (row * TILE_HEIGHT);
        
        struct Image* current_icon;
        switch (state->items[i].icon_index) {
            case 0: current_icon = icon_folder; break;
            case 1: current_icon = icon_file; break;
            case 2: current_icon = icon_document; break;
            case 3: current_icon = icon_system; break;
            case 4: current_icon = icon_storage; break;
            default: current_icon = 0; break;
        }
        
        if (current_icon != NULL) {
            uint16_t icon_offset_x = (TILE_WIDTH - current_icon->width) / 2; 
            dwm_draw_sprite(sp_x + icon_offset_x, sp_y, current_icon);
        }
        
        size_t length = strlen(state->items[i].name);
        uint16_t string_width = length * 6;
        int16_t text_start_x = sp_x + ((TILE_WIDTH - string_width) / 2);
        dwm_draw_text(text_start_x, sp_y + 42, state->items[i].name, item_text);
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
    case EVENT_MOUSE:
        handle_explorer_mouse(handle, state, wparam, lparam);
        break;
        
    case EVENT_RESIZE:
        handle_explorer_resize(state, wparam);
        break;
        
    case EVENT_REDRAW:
        handle_explorer_redraw(handle, state);
        break;
        
    case EVENT_DESTROY:
        free_window_state(handle);
        return; 
    }
}
