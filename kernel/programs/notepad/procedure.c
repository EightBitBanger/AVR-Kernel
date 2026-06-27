#include <stdio.h>
#include <stdbool.h>

#include <kernel/kernel.h>
#include <kernel/dwm/dwm.h>
#include <kernel/events.h>
#include <kernel/util/string.h>

#include <kernel/programs/notepad/internal.h>

// Matching definitions from the kernel driver mappings
#define KBD_CUSTOM_BACKSPACE    0x01
#define KBD_CUSTOM_ENTER        0x02

// Native PS/2 Scan Code Set 1 Arrow Keys
#define KBD_SCANCODE_UP         0x48
#define KBD_SCANCODE_LEFT       0x4B
#define KBD_SCANCODE_RIGHT      0x4D
#define KBD_SCANCODE_DOWN       0x50

// Menu Bar Layout Geometry
#define MENUBAR_HEIGHT          16
#define MENUBAR_TEXT_X          10
#define MENUBAR_TEXT_Y          4
#define MENUBAR_CLICK_WIDTH     36 

static struct NotepadWindowState* get_notepad_window_state(WindowHandle handle) {
    struct NotepadWindowState* current = notepad_window_list_head;
    while (current != NULL) {
        if (current->handle == handle) return current;
        current = current->next;
    }
    return NULL;
}

static void handle_notepad_resize(struct NotepadWindowState* state, uint32_t wparam) {
    state->win_width = (uint16_t)(wparam & 0xFFFF);
    state->win_height = (uint16_t)((wparam >> 16) & 0xFFFF);
}

// Helper to calculate current row and column of a given index
static void get_cursor_layout_pos(struct NotepadWindowState* state, uint16_t max_chars, uint32_t target_idx, uint16_t* out_row, uint16_t* out_col) {
    uint16_t r = 0, c = 0;
    for (uint32_t i = 0; i < target_idx && i < state->text_length; i++) {
        if (state->text_buffer[i] == '\n') {
            r++;
            c = 0;
        } else {
            c++;
            if (c >= max_chars) {
                r++;
                c = 0;
            }
        }
    }
    *out_row = r;
    *out_col = c;
}

// Find a buffer index based on target layout coordinates
static uint32_t get_index_from_layout_pos(struct NotepadWindowState* state, uint16_t max_chars, uint16_t target_row, uint16_t target_col) {
    uint16_t r = 0, c = 0;
    uint32_t last_valid_on_row = 0;
    
    for (uint32_t i = 0; i <= state->text_length; i++) {
        if (r == target_row && c == target_col) {
            return i;
        }
        
        if (i == state->text_length) {
            if (r == target_row) return i;
            break;
        }
        
        if (r == target_row) {
            last_valid_on_row = i;
        }
        
        char target = state->text_buffer[i];
        if (target == '\n') {
            if (r == target_row) {
                // If column is past line length, snap to the newline character
                return i;
            }
            r++;
            c = 0;
        } else {
            c++;
            if (c >= max_chars) {
                if (r == target_row) return i;
                r++;
                c = 0;
            }
        }
    }
    
    if (target_row > r) {
        return state->text_length;
    }
    
    return last_valid_on_row;
}

static void handle_notepad_keypress(WindowHandle handle, struct NotepadWindowState* state, uint32_t wparam) {
    // Extract both ASCII character representation and raw fallback scan codes
    char ascii_char = (char)(wparam & 0xFF);
    uint16_t scancode = (uint8_t)((wparam >> 8) & 0xFF);
    
    // Fallback If wparam contains a packed scancode in the upper byte,
    // or if ascii_char is 0, extract the raw scancode directly from wparam.
    if (scancode == 0 && ascii_char == 0) {
        scancode = (uint8_t)(wparam & 0xFF); 
    }
    
    // Handle Arrow Key Navigation
    if (ascii_char == 0 || scancode == KBD_SCANCODE_LEFT || scancode == KBD_SCANCODE_RIGHT || 
        scancode == KBD_SCANCODE_UP || scancode == KBD_SCANCODE_DOWN) {
        
        uint16_t window_width = dwm_window_get_width(handle);
        uint16_t max_chars = (window_width - (TEXT_PADDING_X * 2)) / TEXT_FONT_CHAR_WIDTH;
        if (max_chars == 0) max_chars = 1;
        
        uint16_t cur_row = 0, cur_col = 0;
        get_cursor_layout_pos(state, max_chars, state->cursor_index, &cur_row, &cur_col);
        
        switch (scancode) {
            case KBD_SCANCODE_LEFT:
                if (state->cursor_index > 0) {
                    state->cursor_index--;
                    dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                }
                return;
                
            case KBD_SCANCODE_RIGHT:
                if (state->cursor_index < state->text_length) {
                    state->cursor_index++;
                    dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                }
                return;
                
            case KBD_SCANCODE_UP:
                if (cur_row > 0) {
                    state->cursor_index = get_index_from_layout_pos(state, max_chars, cur_row - 1, cur_col);
                    dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                }
                return;
                
            case KBD_SCANCODE_DOWN:
                // Move down if there's a subsequent line to navigate to
                state->cursor_index = get_index_from_layout_pos(state, max_chars, cur_row + 1, cur_col);
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                return;
        }
    }
    
    // Process Backspace at cursor position
    if (ascii_char == KBD_CUSTOM_BACKSPACE) {
        if (state->cursor_index > 0 && state->text_length > 0) {
            for (uint32_t i = state->cursor_index - 1; i < state->text_length; i++) {
                state->text_buffer[i] = state->text_buffer[i + 1];
            }
            state->text_length--;
            state->cursor_index--;
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
        return;
    }
    
    // Process standard inputs if buffer space permits
    if (state->text_length < (MAX_TEXT_LEN - 1)) {
        if (ascii_char == KBD_CUSTOM_ENTER) {
            ascii_char = '\n';
        }
        
        if ((ascii_char >= ' ' && ascii_char <= '~') || ascii_char == '\n' || ascii_char == '\t') {
            for (uint32_t i = state->text_length; i > state->cursor_index; i--) {
                state->text_buffer[i] = state->text_buffer[i - 1];
            }
            
            state->text_buffer[state->cursor_index] = ascii_char;
            state->text_length++;
            state->cursor_index++;
            state->text_buffer[state->text_length] = '\0';
            
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
    }
}

static void handle_notepad_mouse(WindowHandle handle, struct NotepadWindowState* state, uint32_t wparam, int32_t lparam) {
    uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
    uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);
    
    const char* file_menu_options[] = { "New", "Open", "Save", "Clear", "Exit" };
    const char* canvas_menu_options[] = { "Cut", "Copy", "Delete", "Paste" };
    
    // Right-click context menu
    if (lparam & DWM_STATE_MOUSE_BTN_RIGHT) {
        context_directive = 0;
        dwm_summon_context_menu(handle, click_x, click_y, canvas_menu_options, 4);
        return;
    }
    
    // Left-click processing
    if (lparam & DWM_STATE_MOUSE_BTN_LEFT) {
        
        uint16_t titlebar_height = dwm_get_titlebar_height(handle);
        
        uint16_t btn_x = MENUBAR_TEXT_X;
        uint16_t btn_y = 0;
        uint16_t btn_w = MENUBAR_CLICK_WIDTH;
        uint16_t btn_h = MENUBAR_HEIGHT;
        
        // Standard AABB Hit Test
        if (click_x >= btn_x && click_x < (btn_x + btn_w) &&
            click_y >= btn_y && click_y < (btn_y + btn_h)) {
            
            // Summon menu directly anchored to the bottom-left of the button bounds
            context_directive = 1;
            dwm_summon_context_menu(handle, btn_x, btn_y + btn_h, file_menu_options, 5);
            return;
        }
        
        // Canvas Text Index Selection
        uint16_t text_top_y = TEXT_PADDING_Y + MENUBAR_HEIGHT;
        if (click_y >= text_top_y) {
            uint16_t rel_x = (click_x >= TEXT_PADDING_X) ? (click_x - TEXT_PADDING_X) : 0;
            uint16_t rel_y = click_y - text_top_y;
            
            uint16_t target_row = rel_y / TEXT_FONT_LINE_HEIGHT;
            uint16_t target_col = rel_x / TEXT_FONT_CHAR_WIDTH;
            
            uint16_t window_width = dwm_window_get_width(handle);
            uint16_t max_chars_per_line = (window_width - (TEXT_PADDING_X * 2)) / TEXT_FONT_CHAR_WIDTH;
            if (max_chars_per_line == 0) max_chars_per_line = 1;
            
            state->cursor_index = get_index_from_layout_pos(state, max_chars_per_line, target_row, target_col);
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
    }
}

static void handle_notepad_redraw(WindowHandle handle, struct NotepadWindowState* state) {
    uint16_t window_width = dwm_window_get_width(handle);
    uint16_t window_height = dwm_window_get_height(handle);
    
    dwm_draw_rect_filled(NOTEPAD_BG_X, NOTEPAD_BG_Y, window_width, window_height, notepad_bg);
    
    // Draw Menu Bar Accent
    dwm_draw_rect_filled(0, 0, window_width, MENUBAR_HEIGHT, 0xFF1C1C2A);
    dwm_draw_line(0, MENUBAR_HEIGHT - 1, window_width, 0, 0xFF444466);
    dwm_draw_text(MENUBAR_TEXT_X, MENUBAR_TEXT_Y, "File", 0xFFFFFFFF);
    
    // Layout boundaries Setup
    uint16_t max_chars_per_line = (window_width - (TEXT_PADDING_X * 2)) / TEXT_FONT_CHAR_WIDTH;
    if (max_chars_per_line == 0) max_chars_per_line = 1;
    
    uint16_t current_row = 0;
    uint16_t current_col = 0;
    
    char line_scratchpad[128];
    uint16_t scratch_idx = 0;
    uint16_t dynamic_start_y = TEXT_PADDING_Y + MENUBAR_HEIGHT;
    
    int16_t cursor_render_x = TEXT_PADDING_X;
    int16_t cursor_render_y = dynamic_start_y;
    
    for (uint32_t i = 0; i <= state->text_length; i++) {
        if (i == state->cursor_index) {
            cursor_render_x = TEXT_PADDING_X + (current_col * TEXT_FONT_CHAR_WIDTH);
            cursor_render_y = dynamic_start_y + (current_row * TEXT_FONT_LINE_HEIGHT);
        }
        
        if (i == state->text_length) break;
        
        char target = state->text_buffer[i];
        bool force_flush = false;
        
        if (target == '\n') {
            force_flush = true;
        } else {
            line_scratchpad[scratch_idx++] = target;
            current_col++;
            
            if (current_col >= max_chars_per_line || scratch_idx >= (sizeof(line_scratchpad) - 1)) {
                force_flush = true;
            }
        }
        
        if (force_flush || i == (state->text_length - 1)) {
            if (scratch_idx > 0) {
                line_scratchpad[scratch_idx] = '\0';
                int16_t render_x = TEXT_PADDING_X;
                int16_t render_y = dynamic_start_y + (current_row * TEXT_FONT_LINE_HEIGHT);
                dwm_draw_text(render_x, render_y, line_scratchpad, notepad_text_color);
            }
            
            scratch_idx = 0;
            if (target == '\n' || force_flush) {
                current_col = 0;
                current_row++;
            }
        }
    }
    
    dwm_draw_line(cursor_render_x, cursor_render_y, 0, TEXT_FONT_LINE_HEIGHT, 0xFF3FFF3F);
}

void callback_handler_notepad(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct NotepadWindowState* state = get_notepad_window_state(handle);
    if (!state) return;
    
    switch (event) {
    case DWM_EVENT_MOUSE:
        handle_notepad_mouse(handle, state, wparam, lparam);
        break;
        
    case DWM_EVENT_KEYBOARD:
        handle_notepad_keypress(handle, state, wparam);
        break;
        
    case DWM_EVENT_RESIZE:
        handle_notepad_resize(state, wparam);
        break;
        
    case DWM_EVENT_REDRAW:
        handle_notepad_redraw(handle, state);
        break;
        
    case DWM_EVENT_DESTROY:
        free_notepad_window_state(handle);
        dwm_window_send_event(handle, DWM_EVENT_CLOSE);
        return;
        
    case DWM_EVENT_CONTEXT_MENU:
        
        if (context_directive == CONTEXT_DIRECTIVE_FILE) {
            switch (wparam) {
                
            case 0: // New
                dwm_summon_message_box("File Menu", "Creating a new file...");
                break;
                
            case 1: // Open
                dwm_summon_message_box("File Menu", "Opening file dialog...");
                break;
                
            case 2: // Save
                
                File file = vfs_open(state->file_path, VFS_OPEN_WRITE);
                if (file != INVALID_FILE_ID) {
                    uint32_t size = vfs_get_size(file);
                    
                    // Check file resize
                    if (state->text_length > size) {
                        size = state->text_length;
                        vfs_close(file);
                        
                        // Resize file
                        vfs_truncate(state->file_path, state->text_length);
                        
                        file = vfs_open(state->file_path, VFS_OPEN_WRITE);
                        if (file == INVALID_FILE_ID) {
                            if (!vfs_mkfile(state->file_path, size)) {
                                dwm_summon_message_box("File Menu", "Unable to save file");
                            } else {
                                // Open new file
                                file = vfs_open(state->file_path, VFS_OPEN_WRITE);
                            }
                        }
                    }
                    
                    vfs_write(file, state->text_buffer, size);
                    vfs_close(file);
                }
                break;
                
            case 3: // Clear
                state->text_length = 0;
                state->cursor_index = 0;
                state->text_buffer[0] = '\0';
                dwm_window_send_event(handle, DWM_EVENT_REDRAW);
                break;
                
            case 4: // Exit
                dwm_window_send_event(handle, DWM_EVENT_DESTROY);
                break;
            
            }
        } else if (context_directive == CONTEXT_DIRECTIVE_CANVAS) {
            switch (wparam) {
                
            case 0: // Cut
                dwm_summon_message_box("Canvas", "Cut");
                break;
                
            case 1: // Copy
                dwm_summon_message_box("Canvas", "Copy");
                break;
                
            case 2: // Delete
                dwm_summon_message_box("Canvas", "Delete");
                break;
                
            case 3: // Paste
                dwm_summon_message_box("Canvas", "Paste");
                break;
                
            }
            
        }
        return;
    }
}
