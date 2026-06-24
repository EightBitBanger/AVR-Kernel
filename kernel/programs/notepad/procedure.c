#include <stdio.h>
#include <stdbool.h>

#include <kernel/kernel.h>
#include <kernel/dwm/dwm.h>
#include <kernel/events.h>
#include <kernel/util/string.h>

#include <kernel/programs/notepad/internal.h>

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

static void handle_notepad_keypress(WindowHandle handle, struct NotepadWindowState* state, uint32_t wparam) {
    char ascii_char = (char)(wparam & 0xFF);
    
    // Process Backspace
    if (ascii_char == '\b') {
        if (state->text_length > 0) {
            state->text_length--;
            state->text_buffer[state->text_length] = '\0';
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
        return;
    }
    
    // Process standard inputs if buffer space permits
    if (state->text_length < (MAX_TEXT_LEN - 1)) {
        // Simple filter for common typable text characters and carriage returns
        if ((ascii_char >= ' ' && ascii_char <= '~') || ascii_char == '\n' || ascii_char == '\t') {
            state->text_buffer[state->text_length] = ascii_char;
            state->text_length++;
            state->text_buffer[state->text_length] = '\0';
            
            dwm_window_send_event(handle, DWM_EVENT_REDRAW);
        }
    }
}

static void handle_notepad_redraw(WindowHandle handle, struct NotepadWindowState* state) {
    uint16_t window_width = dwm_window_get_width(handle);
    uint16_t window_height = dwm_window_get_height(handle);
    
    // Blank canvas area to background hex tone
    dwm_draw_rect_filled(NOTEPAD_BG_X, NOTEPAD_BG_Y, window_width, window_height, notepad_bg);
    
    // Calculate boundaries for text matrix mapping
    uint16_t max_chars_per_line = (window_width - (TEXT_PADDING_X * 2)) / TEXT_FONT_CHAR_WIDTH;
    if (max_chars_per_line == 0) max_chars_per_line = 1;
    
    uint16_t current_row = 0;
    uint16_t current_col = 0;
    
    char line_scratchpad[128];
    uint16_t scratch_idx = 0;
    
    // Stream render character elements safely mapped to window size properties
    for (uint32_t i = 0; i < state->text_length; i++) {
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
        
        // Print completed line row
        if (force_flush || i == (state->text_length - 1)) {
            if (scratch_idx > 0) {
                line_scratchpad[scratch_idx] = '\0';
                
                int16_t render_x = TEXT_PADDING_X;
                int16_t render_y = TEXT_PADDING_Y + (current_row * TEXT_FONT_LINE_HEIGHT);
                
                dwm_draw_text(render_x, render_y, line_scratchpad, notepad_text_color);
            }
            
            scratch_idx = 0;
            current_col = 0;
            if (target == '\n' || force_flush) {
                current_row++;
            }
        }
    }
}

void callback_handler_notepad(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct NotepadWindowState* state = get_notepad_window_state(handle);
    if (!state) return;
    
    switch (event) {
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
        return;
    }
}
