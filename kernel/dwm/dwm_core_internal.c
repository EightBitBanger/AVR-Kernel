#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/console/display.h>

#include <kernel/util/string.h>
#include <kernel/util/list.h>


bool dwm_create_context_menu(int x, int y, uint32_t directive, const char* items[], int item_count) {
    if (item_count <= 0 || item_count > 16) { 
        return false;
    }
    
    // Reset or start at the root menu layer (layer 0)
    ctxmenu.menu_count = 1;
    struct ContextMenu* menu = &ctxmenu.menus[0];
    
    // Set up standard menu dimensions and styling details
    menu->visible = true;
    menu->x = x;
    menu->y = y;
    menu->item_height = 22;
    menu->item_count = item_count;
    menu->w = 130; // Standard base width, could dynamically scale if needed
    menu->h = menu->item_height * menu->item_count;
    menu->hovered_item = -1; // Clear hover state on initial pop
    
    ctxmenu.menu_directive = directive;
    
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
    if (menu->y + menu->h > display_h - taskbar.height) {
        menu->y = display_h - taskbar.height - menu->h;
    }
    
    // Redraw the context menu area
    dwm_invalidate_region(menu->x, menu->y, menu->w, menu->h);
    
    return true;
}

struct WindowObject* dwm_get_window_by_id(uint32_t id) {
    if (id == 0) return NULL;
    for (struct list_node* node = workspace.window_head; node != NULL; node = node->next) {
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
    if (target == NULL || (workspace.window_tail != NULL && workspace.window_tail->data == target)) {
        return; 
    }
    
    if (workspace.window_tail != NULL) {
        struct WindowObject* old_active = (struct WindowObject*)workspace.window_tail->data;
        old_active->flags |= DWM_WFLAG_REDECORATE;
        
    }
    
    if (list_remove(&workspace.window_head, &workspace.window_tail, target)) {
        list_append(&workspace.window_head, &workspace.window_tail, target);
    }
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
    if (workspace.window_tail != NULL && (struct WindowObject*)workspace.window_tail->data != window) {
        list_remove(&workspace.window_head, &workspace.window_tail, window);
        list_append(&workspace.window_head, &workspace.window_tail, window);
        
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
    for (struct list_node* node = workspace.window_head; node != NULL; node = node->next) {
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
