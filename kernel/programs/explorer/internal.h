#ifndef PROGRAM_EXPLORER_INTERNAL_H
#define PROGRAM_EXPLORER_INTERNAL_H

#include <kernel/programs/explorer/explorer.h>

extern struct Image* icon_folder;
extern struct Image* icon_file;
extern struct Image* icon_document;
extern struct Image* icon_system;
extern struct Image* icon_storage;

extern struct Image* ui_button_back;
extern struct Image* ui_button_new;

extern uint32_t background;
extern uint32_t path_bg;
extern uint32_t path_border;
extern uint32_t text_knode;
extern uint32_t text_mount;
extern uint32_t navbar_div;
extern uint32_t item_text;

extern struct ExplorerWindowState* window_list_head;

WindowHandle explorer_create_instance(const char* title, uint32_t target_directory, uint32_t target_mount);
struct ExplorerWindowState* allocate_window_state(WindowHandle handle);
void free_window_state(WindowHandle handle);

void populate_state_from_knode(struct ExplorerWindowState* state, uint32_t dir_addr);
void populate_state_from_file_system(struct ExplorerWindowState* state, uint32_t knode_addr, uint32_t fs_dir_addr);

void callback_handler_explorer(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam);

struct Item {
    char name[MAX_TITLE_LEN];          // File/alias name
    char path[MAX_PATH_LEN];           // Truncated path for show
    uint16_t icon_index;               // Icon representing this item
    uint32_t knode;                    // Kernel directory address
    uint32_t fs_dir;                   // Internal File system directory address
};

struct ExplorerWindowState {
    WindowHandle handle;
    EditFieldHandle edit_handle;
    uint16_t edit_width;
    
    uint16_t win_width;
    uint16_t win_height;
    
    uint16_t total_items;
    
    int32_t context_item_index;        // Tracks right-clicked item (-1 means none/blank area)
    uint32_t context_directive;        // Tracks the type of menu that was summoned
    
    uint32_t knode_current;            // Current kernel directory (knode)
    uint32_t fs_current;               // Current internal FS directory (0 if browsing knodes)
    
    struct ExplorerWindowState* next;
    
    struct Item items[MAX_ITEMS];
    
    char window_title[MAX_TITLE_LEN];  // Current directory name as the window title
    char path[MAX_PATH_LEN];           // Truncated path for display
    char full_path[MAX_PATH_LEN];      // Absolute path
    uint16_t knode_path_len;           // Stores length of virtual base path for split rendering
};


#endif
