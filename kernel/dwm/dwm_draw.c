#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/string.h>

bool rects_intersect(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    return !(x1 + w1 <= x2 || x2 + w2 <= x1 || y1 + h1 <= y2 || y2 + h2 <= y1);
}

void get_rect_intersection(int x1, int y1, int w1, int h1, 
                           int x2, int y2, int w2, int h2, 
                           int *out_x, int *out_y, int *out_w, int *out_h) {
    int ix1 = (x1 > x2) ? x1 : x2;
    int iy1 = (y1 > y2) ? y1 : y2;
    int ix2 = (x1 + w1 < x2 + w2) ? x1 + w1 : x2 + w2;
    int iy2 = (y1 + h1 < y2 + h2) ? y1 + h1 : y2 + h2;

    if (ix1 < ix2 && iy1 < iy2) {
        *out_x = ix1;
        *out_y = iy1;
        *out_w = ix2 - ix1;
        *out_h = iy2 - iy1;
    } else {
        *out_w = 0;
        *out_h = 0; // No overlap
    }
}

struct WindowObject* dwm_get_root_parent(struct WindowObject* window) {
    struct WindowObject* root = window;
    while (root->parent != NULL) {
        root = root->parent;
    }
    return root;
}

void dwm_sync_child_positions(struct WindowObject* parent) {
    if (parent == NULL) return;
    
    struct list_node* current = parent->children_head;
    while (current != NULL) {
        struct WindowObject* child = (struct WindowObject*)current->data;
        
        // Update child's absolute positions using its local offsets and parent surface bounds
        child->x = parent->surface_x + child->local_x;
        child->y = parent->surface_y + child->local_y;
        
        // Re-calculate the child's client surface bounds based on its new absolute position
        child->surface_x = child->x;
        child->surface_y = child->y + child->titlebar_height + (child->border_width ? 1 : 0);
        
        // Propagate updates further down the layout tree if this child has children
        dwm_sync_child_positions(child);
        
        current = current->next;
    }
}

void dwm_render_window_recursive(struct WindowObject* window, const struct WindowContext* ctx, uint32_t* frame_buffer, uint32_t screen_stride) {
    if (window == NULL) return;
    
    if (window->flags & WINDOW_FLAG_REDRAW) { 
        window->flags &= ~WINDOW_FLAG_REDRAW;
        
        struct WindowObject* root = dwm_get_root_parent(window);
        
        // Calculate child's position relative to the root parent's surface bounds
        int root_abs_x, root_abs_y, child_abs_x, child_abs_y;
        dwm_get_absolute_position(root, &root_abs_x, &root_abs_y);
        dwm_get_absolute_position(window, &child_abs_x, &child_abs_y);
        
        int relative_x = child_abs_x - root->surface_x;
        int relative_y = child_abs_y - root->surface_y;
        
        // Clip drawing to the child window's visual boundaries on the parent buffer
        draw_set_clip_rect(relative_x, relative_y, window->buffer_w, window->buffer_h);
        
        // Point to the shared memory array
        draw_set_buffer(root->frame_buffer, root->buffer_w, root->buffer_h);
        
        // Shift the drawing origin to the child's relative starting spot!
        // (Adjust this to match your graphics library API)
        draw_set_origin(relative_x, relative_y); 
        
        event_window = window;
        if (window->event_callback != NULL) {
            window->event_callback(window->id, EVENT_REDRAW, 0);
        }
        event_window = NULL;
        
        // Reset drawing state back to defaults
        draw_set_origin(0, 0); 
        draw_set_clip_rect(0, 0, display_get_width(), display_get_height());
        draw_set_buffer_default();
    }
    
    int win_min_x = window->x - window->border_width;
    int win_min_y = window->y - window->border_width;
    int win_w = window->w + (window->border_width * 2);
    int win_h = window->h + (window->border_width * 2);
    
    // Check intersection with dirty regions
    bool dirty_intersection = false;
    for (int i = 0; i < ctx->dirty_count; i++) {
        struct Rect r = ctx->dirty_regions[i];
        if (rects_intersect(r.x, r.y, r.w, r.h, win_min_x, win_min_y, win_w, win_h)) {
            dirty_intersection = true;
            break;
        }
    }
    
    int do_redraw = dirty_intersection || (window->flags & (WINDOW_FLAG_REDRAW | WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE));
    
    // Process window events
    
    dwm_process_window_events( window );
    
    if (do_redraw) {
        
        // Handle application-level redraw request
        if (window->flags & WINDOW_FLAG_REDRAW) { 
            window->flags &= ~WINDOW_FLAG_REDRAW;
            
            draw_set_clip_rect(0, 0, window->buffer_w, window->buffer_h);
            draw_set_buffer(window->frame_buffer, window->buffer_w, window->buffer_h);
            
            event_window = window;
            if (window->event_callback != NULL) {
                window->event_callback(window->id, EVENT_REDRAW, 0);
            }
            event_window = NULL;
            
            draw_set_clip_rect(0, 0, display_get_width(), display_get_height());
            draw_set_buffer_default();
        }
        
        if ((window->flags & (WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE)) || dirty_intersection) {
            
            // Draw decorations (borders/titlebars) onto the screen
            dwm_draw_window(window);
            
            // Upload client pixel contents to the system back-buffer
            if (window->parent == NULL) {
                // Root windows blit their own buffer normally
                int clip_x = window->x; 
                int clip_y = window->y + window->titlebar_height;
                int clip_w = window->w; 
                int clip_h = window->h - window->titlebar_height;
                
                dwm_upload_window_buffer_to_backbuffer(window, frame_buffer, screen_stride, clip_x, clip_y, clip_w, clip_h);
            } else {
                // Child windows force their root ancestor to re-blit the specific region they occupy
                struct WindowObject* root = dwm_get_root_parent(window);
                
                int child_abs_x, child_abs_y;
                dwm_get_absolute_position(window, &child_abs_x, &child_abs_y);
                
                int clip_x = child_abs_x;
                int clip_y = child_abs_y;
                int clip_w = window->buffer_w;
                int clip_h = window->buffer_h;
                
                // Intersect child with parent's surface boundary clip
                get_rect_intersection(clip_x, clip_y, clip_w, clip_h,
                                    window->parent->surface_x, window->parent->surface_y,
                                    window->parent->surface_w, window->parent->surface_h,
                                    &clip_x, &clip_y, &clip_w, &clip_h);
                                    
                if (clip_w > 0 && clip_h > 0) {
                    dwm_upload_window_buffer_to_backbuffer(root, frame_buffer, screen_stride, clip_x, clip_y, clip_w, clip_h);
                }
            }
            
            window->flags &= ~(WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE);
        }
    }
    
    // Draw the children immediately after drawing the base window
    
    struct list_node* current = window->children_head;
    while (current != NULL) {
        struct WindowObject* child = (struct WindowObject*)current->data;
        dwm_render_window_recursive(child, ctx, frame_buffer, screen_stride);
        current = current->next;
    }
}


void dwm_draw_desktop(const struct WindowContext* ctx) {
    
    // Clear out background pixels for all localized dirty regions
    for (int i = 0; i < ctx->dirty_count; i++) {
        struct Rect r = ctx->dirty_regions[i];
        draw_rect_filled(r.x, r.y, r.w, r.h, bg_color);
    }
    
    uint16_t bar_x = 0;
    uint16_t bar_y = display_get_height() - taskbar_height;
    uint16_t bar_w = display_get_width();
    uint16_t bar_h = taskbar_height;
    
    for (int i = 0; i < ctx->dirty_count; i++) {
        struct Rect r = ctx->dirty_regions[i];
        if (rects_intersect(r.x, r.y, r.w, r.h, bar_x, bar_y, bar_w, bar_h)) {
            draw_rect_filled(bar_x, bar_y, bar_w, bar_h, bg_color);
            break;
        }
    }
    
    // Draw desktop icons
    
    struct list_node* current_node = icon_head;
    while (current_node != NULL) {
        struct IconObject* current_icon = (struct IconObject*)current_node->data;
        
        int icon_full_x = current_icon->x + current_icon->bounds_x;
        int icon_full_y = current_icon->y + current_icon->bounds_y;
        
        bool in_refresh_zone = false;
        for (int i = 0; i < ctx->dirty_count; i++) {
            struct Rect r = ctx->dirty_regions[i];
            if (rects_intersect(r.x, r.y, r.w, r.h, icon_full_x, icon_full_y, current_icon->bounds_w, current_icon->bounds_h)) {
                in_refresh_zone = true;
                break;
            }
        }
        
        if (in_refresh_zone) {
            if (current_icon->icon_sprite != NULL) {
                draw_sprite_blend(current_icon->icon_sprite->data, current_icon->width, current_icon->height, current_icon->x, current_icon->y, 0xFF000000);
            } else {
                draw_rect_filled(current_icon->x, current_icon->y, current_icon->width, current_icon->height, 0xFFFFFFFF);
            }
            
            uint16_t text_y = current_icon->y + 45; 
            size_t length = strlen(current_icon->name);
            uint16_t string_width = length * 6;
            int16_t text_start_x = current_icon->x + ((current_icon->width - string_width) / 2);
            
            for (unsigned int i = 0; i < length; i++) {
                draw_glyph(char_rom, current_icon->name[i], text_start_x + (i * 6), text_y, 0xFFFFFFFF, 0xFF000000, 0xFF000000);
            }
        }
        current_node = current_node->next;
    }
    
    extern struct ClippingPlane clipping_plain;
    extern struct MultibootInfo* vinfo;
    extern uint32_t* front_buffer;
    extern uint32_t* back_buffer;
    extern uint32_t* frame_buffer;
    const uint32_t screen_stride = vinfo->framebuffer_pitch / 4;
    
    // Sync positions before drawing
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        if (window->parent == NULL) {
            dwm_sync_child_positions(window);
        }
    }
    
    // Draw windows
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        // Only process root windows that are NOT topmost and NOT children
        if ((window->style & WINDOW_STYLE_TOPMOST) || (window->parent != NULL) || (window->style & WINDOW_STYLE_CHILD)) 
            continue;
        
        dwm_render_window_recursive(window, ctx, frame_buffer, screen_stride);
    }
    
    // Draw top most windows
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        // Only process root windows that ARE topmost and NOT children
        if (!(window->style & WINDOW_STYLE_TOPMOST) || (window->parent != NULL) || (window->style & WINDOW_STYLE_CHILD)) {
            continue;
        }
        
        dwm_render_window_recursive(window, ctx, frame_buffer, screen_stride);
    }
    
    // Context menus
    
    for (uint8_t m = 0; m < context_menu_count; m++) {
        struct ContextMenu* menu = &context_menus[m];
        if (!menu->visible) continue;
        
        uint16_t menu_height = menu->item_height * menu->item_count;
        menu->h = menu_height;
        
        draw_rect_filled(menu->x, menu->y, menu->w, menu_height, menu->color_bg);
        draw_rect(menu->x, menu->y, menu->w, menu_height, menu->color_border);
        
        for (unsigned int i = 0; i < menu->item_count; i++) {
            uint16_t intersector_height = menu->item_height * i;
            uint16_t menu_x = menu->x + 1;
            uint16_t menu_y = menu->y + intersector_height;
            uint16_t menu_w = menu->w - 2;
            uint16_t menu_h = menu->item_height;
            uint16_t text_x = menu_x + 3;
            uint16_t text_y = menu_y + 7;
            
            if ((int)i == menu->hovered_item) {
                draw_rect_filled(menu_x, menu_y + 1, menu_w, menu_h - 1, menu->color_highlight);
            }
            
            if (i > 0 && (int)i != menu->hovered_item && (int)(i - 1) != menu->hovered_item) {
                draw_line(menu_x, menu_y, menu_x + menu_w, menu_y, menu->color_separator);
            }
            
            size_t length = strlen(menu->item[i].name);
            for (unsigned int s = 0; s < length; s++) {
                draw_glyph(char_rom, menu->item[i].name[s], text_x + (s * 6), text_y, menu->color_text, 0xFF000000, 0xFF000000);
            }
        }
    }
    
    // Redraw cursor sprite
    draw_sprite_blend(current_cursor.data, ctx->cursor_width, ctx->cursor_height, ctx->mouse.x, ctx->mouse.y, 0xFF000000);
}

void dwm_draw_window(struct WindowObject* window_handle) {
    if (window_handle->style & WINDOW_STYLE_NOBORDERS) 
        return;
    
    int32_t wx = window_handle->x;
    int32_t wy = window_handle->y;
    int32_t ww = window_handle->w;
    int32_t wh = window_handle->h;
    
    struct WindowObject* focused_window = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    
    uint32_t current_title_color_low = (window_handle == focused_window) ? window_handle->title_color_low : window_handle->inactive_color_low;
    uint32_t current_title_color_high = (window_handle == focused_window) ? window_handle->title_color_high : window_handle->inactive_color_high;
    
    if (window_handle->titlebar_height > 0) {
        draw_rect_filled(wx, wy, ww, window_handle->titlebar_height, current_title_color_low);
        draw_text(wx + 7, wy + 6, window_handle->title, window_handle->title_text_color);
    }
    
    // Draw the window outer borders
    for (uint8_t b = 1; b <= window_handle->border_width; b++) {
        
        draw_rect(window_handle->x - b, window_handle->y - b, window_handle->w + (b * 2), window_handle->h + (b * 2), window_handle->border_color);
    }
    
    // Draw window button
    for (struct list_node* node = window_handle->buttons_head; node != NULL; node = node->next) {
        struct WindowButton* btn = (struct WindowButton*)node->data;
        
        if (btn->img.data == NULL) 
            continue;
        
        int32_t btn_abs_x = wx + btn->x;
        int32_t btn_abs_y = wy + btn->y;
        
        draw_sprite_blend(btn->img.data, btn->img.width, btn->img.height, btn_abs_x, btn_abs_y, 0xFF000000);
    }
}
