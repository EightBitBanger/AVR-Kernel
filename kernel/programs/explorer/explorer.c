#include <stdio.h>
#include <stdbool.h>

#include <kernel/kernel.h>
#include <kernel/dwm/dwm.h>
#include <kernel/events.h>
#include <kernel/util/string.h>

#include <kernel/programs/explorer/internal.h>
#include <kernel/programs/explorer/explorer.h>

#include <kernel/arch/x86/malloc.h>
#include <kernel/knode.h>

struct Image* icon_folder = NULL;
struct Image* icon_file = NULL;
struct Image* icon_document = NULL;
struct Image* icon_system = NULL;
struct Image* icon_storage = NULL;

struct Image* ui_button_back;
struct Image* ui_button_new;

uint32_t background  = 0xFF08080F;
uint32_t path_bg     = 0xFF202020;
uint32_t path_border = 0xFF404040;
uint32_t text_knode  = 0xFF08F008;
uint32_t text_mount  = 0xFFFDA008;
uint32_t navbar_div  = 0xFF086008;
uint32_t item_text   = 0xFFD0D0DF;

struct ExplorerWindowState* window_list_head = NULL;

void explorer_main(const char* arguments) {
    if (!icon_folder)     icon_folder      = dwm_resource_find("icon_folder");
    if (!icon_file)       icon_file        = dwm_resource_find("icon_file");
    if (!icon_system)     icon_system      = dwm_resource_find("icon_system");
    if (!icon_document)   icon_document    = dwm_resource_find("icon_document");
    if (!icon_storage)    icon_storage     = dwm_resource_find("icon_storage");
    
    if (!ui_button_back)  ui_button_back  = dwm_resource_find("ui_back");
    if (!ui_button_new)   ui_button_new   = dwm_resource_find("ui_new");
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    uint32_t current_knode_addr = fs_current.current_directory;
    uint32_t current_fs_addr = 0; 
    
    char window_title[MAX_TITLE_LEN] = "/";
    
    if (arguments != NULL && arguments[0] != '\0') {
        char path_scratch[128];
        strncpy(path_scratch, arguments, sizeof(path_scratch) - 1);
        path_scratch[sizeof(path_scratch) - 1] = '\0';
        
        if (arguments[0] == '/') {
            current_knode_addr = knode_get_root();
        }
        
        char* token = strtok(path_scratch, "/");
        while (token != NULL) {
            if (strcmp(token, "..") == 0) {
                current_knode_addr = knode_get_parent(current_knode_addr);
            } else if (strcmp(token, ".") != 0) {
                uint32_t found_node = knode_find_by_name(current_knode_addr, token);
                if (found_node != KNODE_NULL && found_node != 0) {
                    current_knode_addr = found_node;
                } else {
                    break;
                }
            }
            
            strncpy(window_title, token, MAX_TITLE_LEN - 1);
            window_title[MAX_TITLE_LEN - 1] = '\0';
            
            token = strtok(NULL, "/");
        }
    }
    
    // Check if the final destination knode is a mounted storage directory
    if (current_knode_addr != KNODE_NULL && current_knode_addr != 0) {
        uint8_t flags = kmalloc_get_flags(current_knode_addr);
        if ((flags & KMALLOC_FLAG_DIRECTORY) && (flags & KMALLOC_FLAG_MOUNT)) {
            // Get the partition block base address attached to this knode
            uint32_t device_mount_address = knode_get_reference(current_knode_addr, 0);
            if (device_mount_address != 0) {
                struct FSPartitionBlock partition;
                fs_device_open(device_mount_address, &partition);
                
                // Point to the actual internal root directory of that storage file system
                current_fs_addr = partition.root_directory;
            }
        }
    }
    
    explorer_create_instance(window_title, current_knode_addr, current_fs_addr);
}

struct ExplorerWindowState* allocate_window_state(WindowHandle handle) {
    struct ExplorerWindowState* new_node = (struct ExplorerWindowState*)malloc(sizeof(struct ExplorerWindowState));
    if (!new_node) return NULL; 
    
    memset(new_node, 0, sizeof(struct ExplorerWindowState));
    new_node->handle = handle;
    new_node->win_width = 400; 
    new_node->next = window_list_head;
    window_list_head = new_node;
    
    return new_node;
}

void free_window_state(WindowHandle handle) {
    struct ExplorerWindowState* current = window_list_head;
    struct ExplorerWindowState* previous = NULL;
    
    while (current != NULL) {
        if (current->handle == handle) {
            if (previous == NULL) {
                window_list_head = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

WindowHandle explorer_create_instance(const char* title, uint32_t target_directory, uint32_t target_mount) {
    struct ExplorerWindowState* state = allocate_window_state(0);
    if (state == NULL) return 0;
    
    if (target_mount == 0) {
        populate_state_from_knode(state, target_directory);
    } else {
        populate_state_from_file_system(state, target_directory, target_mount);
    }
    
    WindowClass wclass;
    memset(&wclass, 0x00, sizeof(WindowClass));
    wclass.x = 200;
    wclass.y = 150;
    wclass.width  = 480;
    wclass.height = 340;
    wclass.max_width  = 0;
    wclass.max_height = 0;
    
    size_t title_length = strnlen(title, DWM_FILENAME_LENGTH);
    
    strncpy(wclass.title, title, title_length);
    wclass.title[title_length] = '\0';
    
    WindowHandle window = dwm_create_window(wclass, DWM_WSTYLE_RESIZEABLE, callback_handler_explorer);
    state->handle = window;
    
    dwm_window_set_focus(window);
    return window;
}

void populate_state_from_file_system(struct ExplorerWindowState* state, uint32_t knode_addr, uint32_t fs_dir_addr) {
    if (!state || fs_dir_addr == FS_NULL || fs_dir_addr == 0) return;

    state->knode_current = knode_addr;
    state->fs_current = fs_dir_addr;
    
    uint32_t device_mount_address = knode_get_reference(knode_addr, 0);
    
    struct FSPartitionBlock partition;
    fs_device_open(device_mount_address, &partition);
    
    memset(state->items, 0, sizeof(state->items));
    state->total_items = 0;
    
    char fs_path_accum[MAX_PATH_LEN];
    memset(fs_path_accum, 0, MAX_PATH_LEN);
    
    uint32_t fs_climb = fs_dir_addr;
    uint32_t depth_counter = 0;
    
    while (fs_climb != FS_NULL && fs_climb != 0) {
        char dir_name[MAX_TITLE_LEN];
        fs_file_get_name(fs_climb, dir_name);
        dir_name[MAX_TITLE_LEN - 1] = '\0';
        
        if (fs_climb == fs_dir_addr && state->handle != 0) {
            dwm_window_set_name(state->handle, dir_name);
        }
        
        if (fs_climb != partition.root_directory) {
            char segment[MAX_TITLE_LEN + 2];
            segment[0] = '/';
            strncpy(&segment[1], dir_name, MAX_TITLE_LEN - 1);
            segment[MAX_TITLE_LEN + 1] = '\0';
            
            char transient[MAX_PATH_LEN];
            memset(transient, 0, MAX_PATH_LEN);
            strncpy(transient, fs_path_accum, MAX_PATH_LEN - 1);
            
            strncpy(fs_path_accum, segment, MAX_PATH_LEN - 1);
            size_t seg_len = strlen(segment);
            if (seg_len < MAX_PATH_LEN - 1) {
                strncpy(fs_path_accum + seg_len, transient, MAX_PATH_LEN - 1 - seg_len);
            }
        }
        
        if (fs_climb == partition.root_directory || depth_counter >= MAX_PATH_DEPTH) {
            break;
        }
        
        fs_climb = fs_directory_get_parent(fs_climb);
        depth_counter++;
    }
    
    char base_knode_path[MAX_PATH_LEN];
    memset(base_knode_path, 0, MAX_PATH_LEN);
    
    uint32_t knode_climb = knode_addr;
    uint32_t parent_node = knode_get_parent(knode_climb);
    uint32_t root_node = knode_get_root();
    depth_counter = 0;
    
    struct KernelDirectory mount_dir_meta;
    kmem_read(&mount_dir_meta, knode_climb, sizeof(struct KernelDirectory));
    
    while (parent_node != root_node && parent_node != KNODE_NULL && parent_node != 0) {
        if (depth_counter >= MAX_PATH_DEPTH) break;
        
        struct KernelDirectory current_dir_meta;
        kmem_read(&current_dir_meta, parent_node, sizeof(struct KernelDirectory));
        
        char segment[32];
        segment[0] = '/';
        strncpy(&segment[1], current_dir_meta.name, sizeof(segment) - 2);
        segment[sizeof(segment) - 1] = '\0';
        
        char transient[MAX_PATH_LEN];
        memset(transient, 0, MAX_PATH_LEN);
        strncpy(transient, base_knode_path, MAX_PATH_LEN - 1);
        
        strncpy(base_knode_path, segment, MAX_PATH_LEN - 1);
        size_t seg_len = strlen(segment);
        if (seg_len < MAX_PATH_LEN - 1) {
            strncpy(base_knode_path + seg_len, transient, MAX_PATH_LEN - 1 - seg_len);
        }
        
        parent_node = knode_get_parent(parent_node);
        depth_counter++;
    }
    
    if (base_knode_path[0] == '\0') {
        strncpy(base_knode_path, "/", MAX_PATH_LEN - 1);
    }
    
    state->knode_path_len = (uint16_t)strlen(base_knode_path);
    
    char mount_segment[32];
    mount_segment[0] = '\0';
    if (base_knode_path[1] != '\0') { 
        strncpy(mount_segment, "/", sizeof(mount_segment) - 1);
    }
    strncat(mount_segment, mount_dir_meta.name, sizeof(mount_segment) - strlen(mount_segment) - 1);
    
    char full_fs_path[MAX_PATH_LEN];
    strncpy(full_fs_path, mount_segment, MAX_PATH_LEN - 1);
    strncat(full_fs_path, fs_path_accum, MAX_PATH_LEN - strlen(full_fs_path) - 1);
    
    strncpy(state->path, base_knode_path, MAX_PATH_LEN - 1);
    size_t current_len = strlen(state->path);
    if (current_len < MAX_PATH_LEN - 1) {
        strncat(state->path, full_fs_path, MAX_PATH_LEN - current_len - 1);
    }

    uint32_t ref_count = fs_directory_get_reference_count(fs_dir_addr);
    uint16_t collected = 0;

    for (uint32_t i = 0; i < ref_count && collected < MAX_ITEMS; i++) {
        uint32_t reference = fs_directory_get_reference(fs_dir_addr, i);
        if (reference == FS_NULL) 
            continue;
        
        fs_file_get_name(reference, state->items[collected].name);
        state->items[collected].name[MAX_TITLE_LEN - 1] = '\0';
        
        uint8_t attributes;
        fs_file_get_attributes(reference, &attributes);
        
        if (attributes & FS_ATTRIBUTE_DIRECTORY) {
            state->items[collected].icon_index = ICON_FOLDER;
            state->items[collected].fs_dir = reference; 
        } else {
            state->items[collected].icon_index = ICON_FILE;
            state->items[collected].fs_dir = 0;
        }
        
        collected++;
    }
    
    state->total_items = collected;
}

void populate_state_from_knode(struct ExplorerWindowState* state, uint32_t dir_addr) {
    if (!state || dir_addr == KNODE_NULL || dir_addr == 0) return;
    
    state->knode_current = dir_addr;
    state->fs_current = 0; 
    state->knode_path_len = 0; 
    uint16_t collected_count = 0;
    
    memset(state->items, 0, sizeof(state->items));
    memset(state->path, 0, MAX_PATH_LEN);
    
    uint32_t current_climb = dir_addr;
    uint32_t root_node = knode_get_root();
    
    if (current_climb == root_node) {
        strncpy(state->path, "/", MAX_PATH_LEN - 1);
        if (state->handle != 0) {
            dwm_window_set_name(state->handle, "/");
        }
    } else {
        char temp_path[MAX_PATH_LEN];
        memset(temp_path, 0, MAX_PATH_LEN);
        
        uint32_t depth_counter = 0;
        bool truncated = false;
        bool is_target_dir = true;
        
        while (current_climb != root_node && current_climb != KNODE_NULL && current_climb != 0) {
            if (depth_counter >= MAX_PATH_DEPTH) {
                truncated = true;
                break;
            }
            
            struct KernelDirectory current_dir_meta;
            memset(&current_dir_meta, 0, sizeof(struct KernelDirectory));
            kmem_read(&current_dir_meta, current_climb, sizeof(struct KernelDirectory));
            
            char segment[32];
            memset(segment, 0, sizeof(segment));
            
            segment[0] = '/';
            strncpy(&segment[1], current_dir_meta.name, sizeof(segment) - 2);
            
            if (is_target_dir && state->handle != 0) {
                dwm_window_set_name(state->handle, current_dir_meta.name);
                is_target_dir = false;
            }
            
            char transient[MAX_PATH_LEN];
            memset(transient, 0, MAX_PATH_LEN);
            strncpy(transient, temp_path, MAX_PATH_LEN - 1);
            
            strncpy(temp_path, segment, MAX_PATH_LEN - 1);
            size_t seg_len = strlen(segment);
            if (seg_len < MAX_PATH_LEN - 1) {
                strncpy(temp_path + seg_len, transient, MAX_PATH_LEN - 1 - seg_len);
            }
            
            depth_counter++;
            current_climb = knode_get_parent(current_climb);
        }
        
        if (truncated) {
            char final_truncated_path[MAX_PATH_LEN];
            memset(final_truncated_path, 0, MAX_PATH_LEN);
            strncpy(final_truncated_path, "/...", MAX_PATH_LEN - 1);
            size_t prefix_len = strlen(final_truncated_path);
            strncpy(final_truncated_path + prefix_len, temp_path, MAX_PATH_LEN - 1 - prefix_len);
            strncpy(state->path, final_truncated_path, MAX_PATH_LEN - 1);
        } else {
            strncpy(state->path, temp_path, MAX_PATH_LEN - 1);
        }
    }
    
    for (uint32_t reference_index = 0; collected_count < MAX_ITEMS; reference_index++) {
        uint32_t reference_address = knode_get_reference(dir_addr, reference_index);
        if (reference_address == KNODE_NULL || reference_address == 0) 
            break;
        
        uint8_t flags = kmalloc_get_flags(reference_address);
        uint8_t type = kmalloc_get_type(reference_address);
        
        struct KernelDirectory kdir;
        memset(&kdir, 0x00, sizeof(struct KernelDirectory));
        kmem_read(&kdir, reference_address, sizeof(struct KernelDirectory));
        
        if (kdir.name[0] == '/' && kdir.name[1] == '\0') continue;
        
        strncpy(state->items[collected_count].name, kdir.name, MAX_TITLE_LEN-1);
        state->items[collected_count].name[MAX_TITLE_LEN-1] = '\0';
        state->items[collected_count].knode = reference_address;
        state->items[collected_count].fs_dir = 0; 
        
        if (flags & KMALLOC_FLAG_DIRECTORY) {
            if (flags & KMALLOC_FLAG_MOUNT) {
                state->items[collected_count].icon_index = ICON_STORAGE;
            } else {
                state->items[collected_count].icon_index = ICON_FOLDER;
            }
        } else { 
            if ((type & KMALLOC_TYPE_DEVICE) | (type & KMALLOC_TYPE_DRIVER)) {
                state->items[collected_count].icon_index = ICON_SYSTEM;
            } else {
                state->items[collected_count].icon_index = ICON_FILE;
            }
        }
        
        collected_count++;
    }
    state->total_items = collected_count;
}
