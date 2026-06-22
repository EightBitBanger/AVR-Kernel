#include <kernel/dwm/dwm.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/dwm/dwm_context_menu.h>

#include <kernel/vfs/vfs.h>

#include <kernel/console/mouse.h>
#include <kernel/console/display.h>

#include <kernel/util/string.h>
#include <kernel/util/list.h>
#include <kernel/util/map.h>

extern const uint8_t char_rom[];
uint32_t bg_color = 0xFF0E0E1A;

struct Image current_cursor;

struct list_node* window_head = NULL;
struct list_node* window_tail = NULL;

struct list_node* icon_head = NULL;
struct list_node* icon_tail = NULL;

struct map_node* resource_head = NULL;
struct map_node* resource_tail = NULL;

struct WindowObject* dragged_window = NULL;
int drag_offset_x = 0;
int drag_offset_y = 0;
bool old_left_button_pressed = false;
bool old_right_button_pressed = false;

struct WindowObject* event_window = NULL;
uint32_t next_window_id;

struct IconObject* dragged_icon = NULL;
int icon_drag_offset_x = 0;
int icon_drag_offset_y = 0;

struct IconObject* focused_icon = NULL;

struct IconObject* last_clicked_icon = NULL;
uint32_t last_icon_click_time = 0;

struct WindowObject* resizing_window = NULL;
int resize_offset_x = 0;
int resize_offset_y = 0;

uint16_t cascade_h = 17;
uint16_t cascade_w = 20;

uint16_t cascade_x = 0;
uint16_t cascade_y = 0;

uint16_t cascade_max = 400;

Point mouse_old;

#define MAX_CONTEXT_MENUS 8
struct ContextMenu context_menus[MAX_CONTEXT_MENUS];
uint8_t context_menu_count = 0;
uint16_t context_menu_directive = 0;
struct WindowObject* context_handle;

struct WindowContext window_context;

WindowHandle w_taskbar;
uint16_t taskbar_height = 28;


void dwm_initiate(void) {
    window_context.cursor_width   = 0;
    window_context.cursor_height  = 0;
    window_context.mouse = mouse_get_position();
    window_context.dirty_count = 0;
    
    cascade_x = cascade_w * 3;
    cascade_y = cascade_h * 3;
    
    context_menu_count = 0;
    
    // Setup global default look for the root menu container slot
    for(int i = 0; i < MAX_CONTEXT_MENUS; i++) {
        context_menus[i].visible = false;
        
        context_menus[i].x = 0;
        context_menus[i].y = 0;
        context_menus[i].w = 0;
        context_menus[i].h = 0;
        
        context_menus[i].color_bg         = 0x8F222222;
        context_menus[i].color_border     = 0x8F444444;
        context_menus[i].color_separator  = 0x8F111111;
        context_menus[i].color_highlight  = 0x8F777777;
        context_menus[i].color_text       = 0x8FE0E0E0;
        context_menus[i].hovered_item     = -1;
        context_menus[i].item_height      = 0;
        context_menus[i].item_count       = 0;
    }
    
    next_window_id = 0;
    window_head = NULL;
    window_tail = NULL;
    
    Point display_center;
    display_center.x = display_get_width() / 2;
    display_center.y = display_get_height() / 2;
    
    cascade_max = display_center.x;
    
    mouse_set_position(display_center.x, display_center.y);
    mouse_old = (Point){display_get_width(), display_get_height()};
    
    // Load resources
    // Icons
    dwm_resource_sprite_load("icon_folder",    &rc_icon_folder);
    dwm_resource_sprite_load("icon_file",      &rc_icon_file);
    dwm_resource_sprite_load("icon_document",  &rc_icon_document);
    dwm_resource_sprite_load("icon_system",    &rc_icon_system);
    dwm_resource_sprite_load("icon_storage",   &rc_icon_storage);
    
    // UI
    dwm_resource_sprite_load("ui_close",         &rc_button_close);
    dwm_resource_sprite_load("ui_close_red",     &rc_button_close_red);
    dwm_resource_sprite_load("ui_close_purple",  &rc_button_close_purple);
    dwm_resource_sprite_load("ui_minimize",      &rc_button_minimize);
    dwm_resource_sprite_load("ui_back",          &rc_button_back);
    dwm_resource_sprite_load("ui_new",           &rc_button_plus);
    dwm_resource_sprite_load("ui_button",        &rc_button);
    
    // Mouse cursors
    dwm_resource_sprite_load("cur_edge",    &rc_cursor_edge);
    dwm_resource_sprite_load("cur_pointer", &rc_cursor_pointer);
    dwm_resource_sprite_load("cur_angle",   &rc_cursor_angle);
    
    // Taskbar
    WindowClass wclass_taskbar;
    wclass_taskbar.x = 0;
    wclass_taskbar.y = display_get_height() - taskbar_height;
    wclass_taskbar.width  = display_get_width();
    wclass_taskbar.height = taskbar_height;
    
    w_taskbar = dwm_create_window(wclass_taskbar, DWM_WSTYLE_TOPMOST | DWM_WSTYLE_NOBORDERS | DWM_WSTYLE_NOCLOSEBOX, callback_taskbar_handler);
    
    // Default mouse cursor
    struct Image* def_cursor = dwm_resource_find("cur_pointer"); 
    if (def_cursor) {
        dwm_set_cursor(def_cursor->data, def_cursor->width, def_cursor->height);
    }
    
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), bg_color);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
}

WindowHandle dwm_create_window(WindowClass wclass, uint16_t wstyle, WindowProcedure wproc) {
    struct WindowObject* window = dwm_allocate_window(wclass, wstyle, wproc);
    return window->id;
}

void dwm_destroy_window(WindowHandle handle) {
    if (handle == 0) return;
    
    struct WindowObject* window_handle = NULL;
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* win = (struct WindowObject*)node->data;
        if (win->id == handle) {
            window_handle = win;
            break;
        }
    }
    if (window_handle == NULL) return; 
    
    // Free associated resources
    dwm_window_resource_free_all(window_handle->id);
    
    if (dragged_window == window_handle) dragged_window = NULL;
    if (resizing_window == window_handle) resizing_window = NULL;
    
    int abs_x, abs_y;
    dwm_get_absolute_position(window_handle, &abs_x, &abs_y);
    
    int destroyed_min_x = abs_x - window_handle->border_width;
    int destroyed_max_x = abs_x + window_handle->w + window_handle->border_width;
    int destroyed_min_y = abs_y - window_handle->border_width;
    int destroyed_max_y = abs_y + window_handle->h + window_handle->border_width;
    
    list_remove(&window_head, &window_tail, window_handle);
    
    // Unchain from parent safely if it has one
    if (window_handle->parent != NULL) {
        list_remove(&window_handle->parent->children_head, 
                    &window_handle->parent->children_tail, 
                    window_handle);
        window_handle->parent = NULL;
    }
    
    // Kill all window buttons
    while (window_handle->buttons_head != NULL) {
        struct list_node* btn_node = window_handle->buttons_head;
        struct WindowButton* btn = (struct WindowButton*)btn_node->data;
        
        list_remove(&window_handle->buttons_head, &window_handle->buttons_tail, btn);
        free(btn);
    }
    
    // Kill all children
    while (window_handle->children_head != NULL) {
        struct list_node* child_node = window_handle->children_head;
        struct WindowObject* child = (struct WindowObject*)child_node->data;
        
        list_remove(&window_handle->children_head, &window_handle->children_tail, child);
        child->parent = NULL; 
        
        dwm_destroy_window(child->id);
    }
    
    dwm_invalidate_region(destroyed_min_x, destroyed_min_y, 
                          destroyed_max_x - destroyed_min_x, 
                          destroyed_max_y - destroyed_min_y);
    
    // Shift focus and force repaint of newly active window
    if (window_tail != NULL) {
        struct WindowObject* tail_win = (struct WindowObject*)window_tail->data;
        dwm_set_focus(tail_win); 
        tail_win->flags |= (DWM_WFLAG_REFRESH | DWM_WFLAG_REDECORATE);
        
        int tail_abs_x, tail_abs_y;
        dwm_get_absolute_position(tail_win, &tail_abs_x, &tail_abs_y);
        dwm_invalidate_region(tail_abs_x - tail_win->border_width,
                              tail_abs_y - tail_win->border_width,
                              tail_win->w + (tail_win->border_width * 2),
                              tail_win->h + (tail_win->border_width * 2));
    }
    
    if (window_handle->frame_buffer != NULL) 
        free(window_handle->frame_buffer);
    
    free(window_handle);
}

struct IconObject* dwm_create_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, struct Image* sprite, uint16_t icon_index) {
    struct IconObject* icon = malloc(sizeof(struct IconObject));
    if (icon == NULL) return NULL;
    
    memset(icon, 0x00, sizeof(struct IconObject));
    icon->x = x;
    icon->y = y;
    icon->width = width;
    icon->height = height;
    icon->icon_sprite = sprite;
    icon->flags = 0; 
    icon->bounds_x = -1;
    icon->bounds_y = -1;
    icon->bounds_w = width + 2;
    icon->bounds_h = height + 2;
    
    if (!list_append(&icon_head, &icon_tail, icon)) {
        free(icon);
        return NULL;
    }
    
    dwm_draw_redraw(icon->x, icon->y, icon->width, icon->height);
    return icon;
}

void dwm_destroy_icon(struct IconObject* icon) {
    if (icon == NULL) return;
    
    size_t length = strlen(icon->name);
    uint16_t string_width = length * 6;
    
    int16_t min_x = icon->x + ((icon->width - string_width) / 2);
    int16_t max_x = min_x + string_width;
    if (min_x > icon->x) min_x = icon->x;
    if (max_x < icon->x + icon->width) max_x = icon->x + icon->width;
    
    dwm_invalidate_region(min_x, icon->y, max_x - min_x, icon->height + 40);
    
    bool explicitly_found = false;
    for (struct list_node* node = icon_head; node != NULL; node = node->next) {
        if (node->data == icon) {
            explicitly_found = true;
            break;
        }
    }
    if (!explicitly_found) return;
    
    list_remove(&icon_head, &icon_tail, icon);
    free(icon);
}

struct WindowObject* dwm_allocate_window(WindowClass w_class, uint16_t w_style, WindowProcedure proc) {
    struct WindowObject* window_object = malloc(sizeof(struct WindowObject));
    if (window_object == NULL) 
        return NULL;
    
    memset(window_object, 0x00, sizeof(struct WindowObject));
    
    uint32_t frame_buffer_sz = w_class.width * w_class.height * sizeof(uint32_t);
    
    if (w_style & DWM_WSTYLE_CHILD) {
        window_object->frame_buffer = NULL; 
    } else {
        window_object->frame_buffer = (uint32_t*)malloc(frame_buffer_sz);
        if (window_object->frame_buffer == NULL) {
            free(window_object);
            return NULL;
        }
        memset(window_object->frame_buffer, 0x11, frame_buffer_sz);
    }
    
    uint32_t candidate_id = next_window_id++;
    if (candidate_id == 0) candidate_id = next_window_id++;
    
    bool id_check = true;
    while (id_check) {
        id_check = false;
        for (struct list_node* node = window_head; node != NULL; node = node->next) {
            struct WindowObject* window = (struct WindowObject*)node->data;
            if (window->id == candidate_id) {
                id_check = true;
                candidate_id = next_window_id++;
                if (candidate_id == 0) candidate_id = next_window_id++;
                break;
            }
        }
    }
    
    window_object->id = candidate_id;
    window_object->style = w_style;
    strncpy(window_object->title, w_class.title, DWM_FILENAME_LENGTH);
    
    // Check if the no-borders style is applied
    if (window_object->style & DWM_WSTYLE_NOBORDERS) {
        window_object->border_width = 0;
        window_object->titlebar_height = 0;
    } else {
        window_object->border_width = 1;
        window_object->titlebar_height = 20;
    }
    
    window_object->parent = NULL;
    
    window_object->x = w_class.x;
    window_object->y = (w_class.y < 0) ? 0 : w_class.y;
    window_object->w = w_class.width;
    window_object->h = w_class.height;
    
    // Window cascading
    if (w_style & DWM_WSTYLE_CASCADE) {
        window_object->x = cascade_x;
        window_object->y = cascade_y;
        
        cascade_x += cascade_w;
        cascade_y += cascade_h;
        
        if (cascade_x > cascade_max) {
            cascade_x = cascade_w;
            cascade_y = cascade_h;
        }
    }
    
    window_object->local_x = 0;
    window_object->local_y = 0;
    int border_offset = 0;
    
    window_object->surface_w = window_object->w;
    window_object->surface_h = dragged_window->h - dragged_window->titlebar_height;
    window_object->buffer_w  = w_class.width; 
    window_object->buffer_h  = w_class.height - window_object->titlebar_height - border_offset;
    
    window_object->surface_x = window_object->x;
    window_object->surface_y = window_object->y + window_object->titlebar_height + 1;
    
    window_object->buffer_w = w_class.width;
    window_object->buffer_h = w_class.height;
    
    window_object->max_width = w_class.max_width;
    window_object->max_height = w_class.max_height;
    
    window_object->border_color         = 0xFF2A2A2A;
    window_object->background_color     = 0xFF6F6F6F;
    window_object->title_text_color     = 0xFFEFEFEF;
    
    window_object->title_color_low      = 0xFF008000;
    window_object->title_color_high     = 0xFF001900;
    window_object->inactive_color_low   = 0xFF505050;
    window_object->inactive_color_high  = 0xFF101010;
    
    window_object->flags = DWM_WFLAG_REDRAW | DWM_WFLAG_REFRESH;
    
    window_object->events = 0;
    window_object->event_callback = proc;
    
    if (window_tail != NULL) {
        struct WindowObject* old_active = (struct WindowObject*)window_tail->data;
        
        // Force the old window to redraw its borders/titlebar in an inactive state
        old_active->flags |= DWM_WFLAG_REDECORATE;
        
        int old_abs_x, old_abs_y;
        dwm_get_absolute_position(old_active, &old_abs_x, &old_abs_y);
        dwm_invalidate_region(old_abs_x - old_active->border_width, 
                              old_abs_y - old_active->border_width, 
                              old_active->w + (old_active->border_width * 2), 
                              old_active->h + (old_active->border_width * 2));
    }
    
    if (!list_append(&window_head, &window_tail, window_object)) {
        free(window_object->frame_buffer); // Clean up buffer on failure
        free(window_object);
        return 0;
    }
    
    int redraw_x, redraw_y;
    dwm_get_absolute_position(window_object, &redraw_x, &redraw_y);
    
    dwm_draw_redraw(redraw_x - window_object->border_width, 
                    redraw_y - window_object->border_width, 
                    window_object->w + (window_object->border_width * 2), 
                    window_object->h + (window_object->border_width * 2));
    
    // Window styling
    
    if (!(w_style & DWM_WSTYLE_NOCLOSEBOX)) {
        struct Image* button_close = dwm_resource_find("ui_close_red");
        struct Image* button_minimize  = dwm_resource_find("ui_minimize");
        
        int16_t close_x = w_class.width - button_close->width;
        int16_t close_min = w_class.width - button_close->width - button_minimize->width;
        int16_t vertical = (window_object->titlebar_height / button_close->height) - 1;
        if (button_close != NULL)    window_add_button(window_object, close_x, vertical, button_close->width, button_close->height, DWM_EVENT_CLOSE, button_close);
        if (button_minimize != NULL) window_add_button(window_object, close_min, vertical, button_close->width, button_close->height, DWM_EVENT_MINIMIZE, button_minimize);
    }
    
    if (w_style & DWM_WSTYLE_RESIZEABLE) {
        uint16_t resize_w = 12;
        uint16_t resize_h = 12;
        int16_t resize_x = window_object->w - resize_w;
        int16_t resize_y = window_object->h - resize_h;
        
        window_add_button(window_object, resize_x, resize_y, resize_w, resize_h, DWM_EVENT_RESIZE, NULL);
    }
    
    return window_object;
}

int8_t dwm_create_folder(uint16_t x, uint16_t y, const char* name, const char* path) {
    struct Image* folder_sprite = dwm_resource_find("icon_folder");
    if (folder_sprite == NULL) 
        return -1;
    
    struct IconObject* folder = dwm_create_icon(x, y, folder_sprite->width, folder_sprite->height, folder_sprite, 0);
    size_t name_length = strnlen(name, DWM_FILENAME_LENGTH);
    size_t path_length = strnlen(path, DWM_PATH_LENGTH);
    
    strncpy(folder->name, name, name_length);
    strncpy(folder->path, path, path_length);
    folder->name[name_length] = '\0';
    folder->path[path_length] = '\0';
    
    dwm_calculate_icon_bounds(folder);
    
    dwm_invalidate_region(x + folder->bounds_x, y + folder->bounds_y, folder->bounds_w, folder->bounds_h);
    return 0;
}

int8_t dwm_create_mount(uint16_t x, uint16_t y, const char* name, const char* path) {
    struct Image* mount_sprite = dwm_resource_find("icon_storage");
    if (mount_sprite == NULL) 
        return -1;
    
    struct IconObject* folder = dwm_create_icon(x, y, mount_sprite->width, mount_sprite->height, mount_sprite, 0);
    size_t name_length = strnlen(name, DWM_FILENAME_LENGTH);
    size_t path_length = strnlen(path, DWM_PATH_LENGTH);
    
    strncpy(folder->name, name, name_length);
    strncpy(folder->path, path, path_length);
    folder->name[name_length] = '\0';
    folder->path[path_length] = '\0';
    
    dwm_calculate_icon_bounds(folder);
    
    dwm_invalidate_region(x + folder->bounds_x, y + folder->bounds_y, folder->bounds_w, folder->bounds_h);
    return 0;
}

int8_t dwm_create_file(uint16_t x, uint16_t y, const char* name, const char* path) {
    struct Image* file_sprite = dwm_resource_find("icon_file");
    if (file_sprite == NULL) 
        return -1;
    
    struct IconObject* file = dwm_create_icon(x, y, file_sprite->width, file_sprite->height, file_sprite, 1);
    size_t name_length = strnlen(name, DWM_FILENAME_LENGTH);
    size_t path_length = strnlen(path, DWM_PATH_LENGTH);
    
    strncpy(file->name, name, name_length);
    strncpy(file->path, path, path_length);
    file->name[name_length] = '\0';
    file->path[path_length] = '\0';
    
    dwm_calculate_icon_bounds(file);
    
    dwm_invalidate_region(x + file->bounds_x, y + file->bounds_y, file->bounds_w, file->bounds_h);
    return 0;
}


void dwm_summon_context_menu(WindowHandle window, uint16_t x, uint16_t y, const char** options, uint16_t number_of_items) {
    struct WindowObject* w_object = dwm_get_window_by_id((uint32_t)window);
    if (w_object == NULL) 
        return;
    
    uint16_t posx = w_object->x + x;
    uint16_t posy = w_object->y + y + w_object->titlebar_height;
    
    // Check window bounds
    if ((posx > w_object->x + w_object->w) || (posy > w_object->y + w_object->h)) 
        return;
    
    // Set the window who called this context menu
    context_handle = w_object;
    
    dwm_create_context_menu(posx, posy, DWM_CONTEXT_MENU_USER, options, number_of_items);
}

WindowHandle dwm_summon_message_box(const char* title, const char* message) {
    WindowClass wclass_msgbox;
    uint16_t width  = 300;
    uint16_t height = 150;
    
    // Center the message box on the screen
    wclass_msgbox.width  = width;
    wclass_msgbox.height = height;
    wclass_msgbox.x      = (display_get_width() - width) / 2;
    wclass_msgbox.y      = (display_get_height() - height) / 2;
    
    // Assign boundaries limits
    wclass_msgbox.max_width  = width;
    wclass_msgbox.max_height = height;
    
    // Copy the title safely over to the class context
    strncpy(wclass_msgbox.title, title, DWM_FILENAME_LENGTH - 1);
    wclass_msgbox.title[DWM_FILENAME_LENGTH - 1] = '\0';
    
    struct WindowObject* msg_handle = dwm_allocate_window(
        wclass_msgbox, 
        0, 
        (WindowProcedure)callback_message_box_handler
    );
    
    // Add the message text as a window resource
    size_t message_length = strnlen(message, 32);
    
    char* rc_message = (char*)malloc(message_length);
    strncpy(rc_message, message, message_length);
    rc_message[message_length] = '\0';
    
    dwm_window_resource_add(msg_handle->id, "text", rc_message);
    
    dwm_set_focus(msg_handle);
    
    return msg_handle->id;
}

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
    strncpy(wclass_props.title, title, DWM_FILENAME_LENGTH - 1);
    wclass_props.title[DWM_FILENAME_LENGTH - 1] = '\0';
    
    struct WindowObject* msg_handle = dwm_allocate_window(
        wclass_props, 
        0, 
        (WindowProcedure)callback_properties_handler
    );
    
    // Instance metadata
    char* instance = (char*)malloc(16);
    dwm_window_resource_add(msg_handle->id, "instance", instance);
    instance[0] = 0;
    
    // General - file metadata
    {
    size_t path_length = strnlen(file_path, DWM_PATH_LENGTH);
    size_t name_length = strnlen(name, DWM_PATH_LENGTH);
    
    char* target_path = (char*)malloc(DWM_PATH_LENGTH);
    char* target_name = (char*)malloc(DWM_PATH_LENGTH);
    char* target_type = (char*)malloc(DWM_PATH_LENGTH);
    
    strncpy(target_path, file_path, path_length);
    target_path[path_length] = '\0';
    
    strncpy(target_name, name, name_length);
    target_name[name_length] = '\0';
    
    if (vfs_is_directory(file_path)) {
        
        if (vfs_is_directory_mounted(file_path)) {
            
            strncpy(target_type, "Storage", DWM_PATH_LENGTH);
        } else {
            strncpy(target_type, "Folder", DWM_PATH_LENGTH);
        }
        
    } else {
        
        switch (icon_index) {
        
        case 1: strncpy(target_type, "File", DWM_PATH_LENGTH); break;
        case 2: strncpy(target_type, "Document", DWM_PATH_LENGTH); break;
        case 3: strncpy(target_type, "System", DWM_PATH_LENGTH); break;
        
        }
        
    }
    
    dwm_window_resource_add(msg_handle->id, "path", target_path);
    dwm_window_resource_add(msg_handle->id, "name", target_name);
    dwm_window_resource_add(msg_handle->id, "type", target_type);
    }
    
    // Attributes
    {
    char* target_attrib = (char*)malloc(16);
    memset(target_attrib, ' ', 16);
    
    uint32_t address = resolve_path_to_address(file_path);
    
    uint8_t permissions;
    if (fs_check_directory_valid(address) || fs_file_check(address)) {
        
        fs_file_get_permissions(address, &permissions);
        
    } else {
        
        knode_get_permissions(address, &permissions);
    }
    
    if (permissions & VFS_PERMISSION_EXECUTE) target_attrib[0] = 'x';
    if (permissions & VFS_PERMISSION_READ)    target_attrib[1] = 'r';
    if (permissions & VFS_PERMISSION_WRITE)   target_attrib[2] = 'w';
    
    dwm_window_resource_add(msg_handle->id, "perms", target_attrib);
    
    }
    
    
    
    dwm_set_focus(msg_handle);
    return msg_handle->id;
}

bool dwm_create_context_menu(int x, int y, uint32_t directive, const char* items[], int item_count) {
    if (item_count <= 0 || item_count > 16) { 
        return false;
    }
    
    // Reset or start at the root menu layer (layer 0)
    context_menu_count = 1;
    struct ContextMenu* menu = &context_menus[0];
    
    // Set up standard menu dimensions and styling details
    menu->visible = true;
    menu->x = x;
    menu->y = y;
    menu->item_height = 22;
    menu->item_count = item_count;
    menu->w = 130; // Standard base width, could dynamically scale if needed
    menu->h = menu->item_height * menu->item_count;
    menu->hovered_item = -1; // Clear hover state on initial pop
    
    context_menu_directive = directive;
    
    // Copy item names over safely
    for (int i = 0; i < item_count; i++) {
        // Safe string copy limiting to the ContextMenu item array name buffer limits
        strncpy(menu->item[i].name, items[i], sizeof(menu->item[i].name) - 1);
        menu->item[i].name[sizeof(menu->item[i].name) - 1] = '\0';
    }
    
    // Clamp the menu coordinates so it doesn't spawn off-screen
    int display_w = display_get_width();
    int display_h = display_get_height();
    
    // Taskbar height
    if (menu->x + menu->w > display_w) {
        menu->x = display_w - menu->w;
    }
    if (menu->y + menu->h > display_h - taskbar_height) {
        menu->y = display_h - taskbar_height - menu->h;
    }
    
    // Redraw the context menu area
    dwm_invalidate_region(menu->x, menu->y, menu->w, menu->h);
    
    return true;
}

struct WindowObject* dwm_get_window_by_id(uint32_t id) {
    if (id == 0) return NULL;
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        if (window->id == id) return window;
    }
    return NULL;
}

uint16_t dwm_window_get_width(WindowHandle handle) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) 
        return 0;
    return window->w;
}

uint16_t dwm_window_get_height(WindowHandle handle) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) 
        return 0;
    return window->h;
}

uint8_t dwm_window_set_name(WindowHandle handle, const char* name) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) 
        return 0;
    strncpy(window->title, name, DWM_FILENAME_LENGTH);
    return 1;
}

void dwm_set_focus(struct WindowObject* target) {
    if (target == NULL || (window_tail != NULL && window_tail->data == target)) {
        return; 
    }
    
    if (window_tail != NULL) {
        struct WindowObject* old_active = (struct WindowObject*)window_tail->data;
        old_active->flags |= DWM_WFLAG_REDECORATE;
        
    }
    
    if (list_remove(&window_head, &window_tail, target)) {
        list_append(&window_head, &window_tail, target);
    }
}

void dwm_draw_line(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    draw_line(x, y, x + w, y + h, color);
}

void dwm_draw_text(int16_t x, int16_t y, const char* text, uint32_t color) {
    if (event_window == NULL) return;
    size_t length = strlen(text);
    for (unsigned int i=0; i < length; i++) 
        draw_glyph(char_rom, text[i], 6, 8, x + (i * 6), y, color, 0xFF000000, 0xFF000000);
}

void dwm_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    if (event_window == NULL) return;
    draw_rect(x, y, w, h, color);
}

void dwm_draw_rect_filled(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
    if (event_window == NULL) return;
    draw_rect_filled(x, y, w, h, color);
}

void dwm_draw_rect_filled_gradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color_low, uint32_t color_high) {
    if (event_window == NULL) return;
    draw_rect_gradient_vertical_blend(x, y, w, h, color_high, color_low);
}

void dwm_draw_sprite(int16_t x, int16_t y, struct Image* image) {
    if (event_window == NULL) return;
    draw_sprite_blend(image->data, image->width, image->height, x, y, 0x00000000);
}

void dwm_set_cursor(uint32_t* sprite, int16_t width, int16_t height) {
    current_cursor.data = sprite;
    current_cursor.width = width;
    current_cursor.height = height;
}

void dwm_upload_window_buffer_to_backbuffer(struct WindowObject* window, uint32_t* frame_buffer, uint32_t screen_stride,
                                            int clip_x, int clip_y, int clip_w, int clip_h) {
    int client_start_x = window->x;
    int client_start_y = window->y + window->titlebar_height;
    
    int local_start_x = clip_x - client_start_x;
    int local_start_y = clip_y - client_start_y;
    
    if (local_start_x < 0) { clip_w += local_start_x; local_start_x = 0; }
    if (local_start_y < 0) { clip_h += local_start_y; local_start_y = 0; }
    
    if (local_start_x + clip_w > (int)window->buffer_w) clip_w = (int)window->buffer_w - local_start_x;
    if (local_start_y + clip_h > (int)window->buffer_h) clip_h = (int)window->buffer_h - local_start_y;
    
    if (clip_w <= 0 || clip_h <= 0) return;
    
    for (int i = 0; i < clip_h; i++) {
        int current_local_y  = local_start_y + i;
        int current_screen_y = clip_y + i;
        uint32_t* src  = &window->frame_buffer[(current_local_y * window->buffer_w) + local_start_x];
        uint32_t* dest = &frame_buffer[(current_screen_y * screen_stride) + clip_x];
        int count = clip_w;
        
        asm volatile (
            "cld;\n\t"
            "rep movsd;\n\t"
            : "+S"(src), "+D"(dest), "+c"(count)
            :
            : "memory"
        );
    }
}

void dwm_window_set_parent(WindowHandle child, WindowHandle parent) {
    struct WindowObject* w_parent = dwm_get_window_by_id(parent);
    struct WindowObject* w_child  = dwm_get_window_by_id(child);
    
    if (w_parent == NULL || w_child == NULL) 
        return;
    
    w_child->style |= DWM_WSTYLE_CHILD;
    
    if (w_child->parent != NULL) {
        list_remove(&w_child->parent->children_head, &w_child->parent->children_tail, w_child);
    }
    
    // If local offsets are 0, migrate the window's original creation coordinates
    // to serve as the local relative offsets inside the parent canvas.
    if (w_child->local_x == 0 && w_child->local_y == 0) {
        w_child->local_x = w_child->x;
        w_child->local_y = w_child->y;
    }
    
    w_child->parent = w_parent;
    list_append(&w_parent->children_head, &w_parent->children_tail, w_child);
    
    w_child->x = w_parent->surface_x + w_child->local_x;
    w_child->y = w_parent->surface_y + w_child->local_y;
    
    // Recalculate child's own drawing canvas metrics based on its updated global position
    w_child->surface_x = w_child->x;
    w_child->surface_y = w_child->y + w_child->titlebar_height + (w_child->border_width ? 1 : 0);
    w_child->surface_w = w_child->w;
    w_child->surface_h = w_child->h - w_child->titlebar_height;
    
    // Trigger paint/refresh pipeline using the correct screen coordinates
    w_child->flags |= (DWM_WFLAG_REDRAW | DWM_WFLAG_REFRESH);
    
    dwm_invalidate_region(w_child->x - w_child->border_width, 
                          w_child->y - w_child->border_width, 
                          w_child->w + (w_child->border_width * 2), 
                          w_child->h + (w_child->border_width * 2));
}

WindowHandle dwm_window_get_parent(WindowHandle window) {
    struct WindowObject* handle = dwm_get_window_by_id(window);
    if (handle == NULL) 
        return 0;
    if (handle->parent == NULL) 
        return 0;
    return handle->parent->id;
}

void dwm_window_set_focus(WindowHandle handle) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    // If someone clicks a child window directly, focus its top-level parent instead
    if (window->parent != NULL) {
        window = window->parent;
    }
    
    // Move the parent to the end of the top-level z-order list
    if (window_tail != NULL && (struct WindowObject*)window_tail->data != window) {
        list_remove(&window_head, &window_tail, window);
        list_append(&window_head, &window_tail, window);
        
        // Force the whole group to repaint
        window->flags |= DWM_WFLAG_REFRESH;
        dwm_invalidate_region(window->x - window->border_width, window->y - window->border_width,
                              window->w + (window->border_width * 2), window->h + (window->border_width * 2));
    }
}

void dwm_resource_sprite_load(const char* resource_name, const struct Sprite* sprite) {
    struct Image* img = malloc(sizeof(struct Image));
    if (!img) return;
    
    img->width = sprite->width;
    img->height = sprite->height;
    
    img->data = malloc(sizeof(uint32_t) * img->width * img->height);
    if (!img->data) {
        free(img);
        return;
    }
    
    sprite_get_bitmap(img->data, sprite);
    dwm_resource_load(resource_name, img);
}

void dwm_window_send_event(WindowHandle handle, wEvent event) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    if (window == NULL) return;
    
    window->events |= event;
}

uint32_t dwm_window_get_count(void) {
    uint32_t count = 0;
    
    // Iterate through the master window linked list starting at the head
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        count++;
    }
    
    return count;
}

void window_add_button(struct WindowObject* window, int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t event, struct Image* sprite) {
    if (window == NULL) return;
    
    struct WindowButton* button = malloc(sizeof(struct WindowButton));
    if (button == NULL) return;
    
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;
    
    button->event = event;
    if (sprite == NULL) {
        button->sprite.data = NULL;
    } else {
        button->sprite = *sprite;
    }
    
    list_append(&window->buttons_head, &window->buttons_tail, button);
}
