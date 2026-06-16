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
        uint16_t btn_x1 = BUTTON_BACK_X;
        uint16_t btn_y1 = BUTTON_BACK_Y;
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
    
    uint16_t max_cols = (state->win_width - NAVBAR_X) / NAVBAR_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t tile_start_x = NAVBAR_X + (col * NAVBAR_WIDTH);
        uint16_t tile_start_y = NAVBAR_Y + (row * NAVBAR_HEIGHT);
        
        if (click_x >= tile_start_x && click_x < (tile_start_x + NAVBAR_WIDTH) &&
            click_y >= tile_start_y && click_y < (tile_start_y + NAVBAR_HEIGHT)) {
            
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
    dwm_draw_rect_filled(EXPLORER_BG_X, EXPLORER_BG_Y, window_width, window_height, background);
    
    int16_t path_text_x = 0;
    if (ui_button_back != NULL) {
        path_text_x = BUTTON_BACK_X + ui_button_back->width + BACK_BTN_PADDING_RIGHT;
    }
    
    if (ui_button_back != NULL) {
        dwm_draw_rect_filled(BACK_BTN_CONTAINER_X, BACK_BTN_CONTAINER_Y, BACK_BTN_CONTAINER_W(path_text_x), BACK_BTN_CONTAINER_H, path_bg);
        dwm_draw_rect(BACK_BTN_BORDER_X, BACK_BTN_BORDER_Y, BACK_BTN_BORDER_W(path_text_x), BACK_BTN_BORDER_H, path_border);
        dwm_draw_sprite(BACK_BTN_SPRITE_X, BACK_BTN_SPRITE_Y, ui_button_back);
    }
    
    dwm_draw_rect_filled(PATH_FIELD_BG_X(path_text_x), PATH_FIELD_BG_Y, PATH_FIELD_W(path_text_x, window_width), PATH_FIELD_BG_H, path_bg);
    dwm_draw_rect(PATH_FIELD_BORDER_X(path_text_x), PATH_FIELD_BORDER_Y, PATH_FIELD_W(path_text_x, window_width), PATH_FIELD_BORDER_H, path_border);
    
    // Color segmented path rendering mechanics
    if (state->fs_current == 0 || state->knode_path_len >= strlen(state->path)) {
        dwm_draw_text(path_text_x, PATH_TEXT_Y, state->path, text_knode);
    } else {
        // Split View: Read slice limits to split string processing
        char base_buffer[MAX_PATH_LEN];
        memset(base_buffer, 0, MAX_PATH_LEN);
        strncpy(base_buffer, state->path, state->knode_path_len);
        
        // Draw Virtual KNode sequence
        dwm_draw_text(path_text_x, PATH_TEXT_Y, base_buffer, text_knode);
        
        // Calculate pixel offset (assuming standard 6px per char font width)
        int16_t mount_offset_x = path_text_x + (state->knode_path_len * PATH_FONT_CHAR_WIDTH);
        
        // Draw the rest of the file system path
        dwm_draw_text(mount_offset_x, PATH_TEXT_Y, state->path + state->knode_path_len, text_mount);
    }
    
    // Navigation bar divider
    dwm_draw_line(NAV_DIVIDER_X, NAV_DIVIDER_Y, window_width, NAV_DIVIDER_H, navbar_div);
    
    uint16_t max_cols = (state->win_width - NAVBAR_X) / NAVBAR_WIDTH;
    if (max_cols == 0) max_cols = 1;
    
    for (unsigned int i = 0; i < state->total_items; i++) {
        uint16_t col = i % max_cols;
        uint16_t row = i / max_cols;
        
        uint16_t sp_x = NAVBAR_X + (col * NAVBAR_WIDTH);
        uint16_t sp_y = NAVBAR_Y + (row * NAVBAR_HEIGHT);
        
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
            uint16_t icon_offset_x = (NAVBAR_WIDTH - current_icon->width) / 2; 
            dwm_draw_sprite(sp_x + icon_offset_x, sp_y, current_icon);
        }
        
        size_t length = strlen(state->items[i].name);
        uint16_t string_width = length * ITEM_FONT_CHAR_WIDTH;
        int16_t text_start_x = sp_x + ((NAVBAR_WIDTH - string_width) / 2);
        dwm_draw_text(text_start_x, sp_y + ITEM_TEXT_Y_OFF, state->items[i].name, item_text);
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
