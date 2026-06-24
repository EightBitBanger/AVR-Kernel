#ifndef PROGRAM_NOTEPAD_INTERNAL_H
#define PROGRAM_NOTEPAD_INTERNAL_H

#include <kernel/programs/notepad/notepad.h>

#define MAX_TEXT_LEN              2048
#define MAX_TITLE_LEN               32

extern uint32_t notepad_bg;
extern uint32_t notepad_text_color;

struct NotepadWindowState {
    WindowHandle handle;
    
    uint16_t win_width;
    uint16_t win_height;
    
    char window_title[MAX_TITLE_LEN];
    char text_buffer[MAX_TEXT_LEN];
    uint32_t text_length;
    
    struct NotepadWindowState* next;
};

extern struct NotepadWindowState* notepad_window_list_head;

WindowHandle notepad_create_instance(const char* title);
struct NotepadWindowState* allocate_notepad_window_state(WindowHandle handle, const char* path);
void free_notepad_window_state(WindowHandle handle);

void callback_handler_notepad(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);

#endif
