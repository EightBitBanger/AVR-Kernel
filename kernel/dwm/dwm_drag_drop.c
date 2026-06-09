#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/string.h>

void dwm_update_icon_dragging(struct WindowContext* ctx) {
    if (dragged_icon == NULL) return;
    
    if (ctx->left_button_pressed) {
        int new_x = ctx->mouse.x - icon_drag_offset_x;
        int new_y = ctx->mouse.y - icon_drag_offset_y;
        int display_w = display_get_width();
        int display_h = display_get_height();
        
        if (new_x + dragged_icon->bounds_x < 0) new_x = -dragged_icon->bounds_x;
        if (new_x + dragged_icon->bounds_x + dragged_icon->bounds_w > display_w) new_x = display_w - dragged_icon->bounds_x - dragged_icon->bounds_w;
        if (new_y + dragged_icon->bounds_y < 0) new_y = -dragged_icon->bounds_y;
        if (new_y + dragged_icon->bounds_y + dragged_icon->bounds_h > display_h) new_y = display_h - dragged_icon->bounds_y - dragged_icon->bounds_h;
        
        if (dragged_icon->x != new_x || dragged_icon->y != new_y) {
            dwm_invalidate_region(dragged_icon->x + dragged_icon->bounds_x, dragged_icon->y + dragged_icon->bounds_y, dragged_icon->bounds_w, dragged_icon->bounds_h);
            
            dragged_icon->x = new_x;
            dragged_icon->y = new_y;
            
            dwm_invalidate_region(dragged_icon->x + dragged_icon->bounds_x, dragged_icon->y + dragged_icon->bounds_y, dragged_icon->bounds_w, dragged_icon->bounds_h);
        }
    } else {
        
        dragged_icon = NULL;
    }
}

void dwm_update_window_dragging(struct WindowContext* ctx) {
    if (dragged_window == NULL) return;
    
    if (ctx->left_button_pressed) {
        int new_x = ctx->mouse.x - drag_offset_x;
        int new_y = ctx->mouse.y - drag_offset_y;
        
        int display_w = display_get_width();
        int display_h = display_get_height();
        
        // Constrain window dragging boundaries
        if (new_x - (int)dragged_window->border_width < 0) new_x = dragged_window->border_width;
        if (new_x + (int)dragged_window->w + (int)dragged_window->border_width > display_w) new_x = display_w - dragged_window->w - dragged_window->border_width;
        if (new_y - (int)dragged_window->border_width < 0) new_y = dragged_window->border_width;
        if (new_y + (int)dragged_window->h + (int)dragged_window->border_width > display_h) new_y = display_h - dragged_window->h - dragged_window->border_width;
        
        if (dragged_window->x != new_x || dragged_window->y != new_y) {
            dwm_invalidate_region(dragged_window->x - dragged_window->border_width, dragged_window->y - dragged_window->border_width, dragged_window->w + (dragged_window->border_width * 2), dragged_window->h + (dragged_window->border_width * 2));
            
            dragged_window->x = new_x;
            dragged_window->y = new_y;
            
            if (dragged_window->parent != NULL) {
                dragged_window->local_x = new_x - dragged_window->parent->surface_x;
                dragged_window->local_y = new_y - dragged_window->parent->surface_y;
            } else {
                dragged_window->local_x = 0;
                dragged_window->local_y = 0;
            }
            
            dragged_window->surface_x = new_x;
            dragged_window->surface_y = new_y + dragged_window->titlebar_height + 1;
            dragged_window->surface_w = dragged_window->w;
            dragged_window->surface_h = dragged_window->h - dragged_window->titlebar_height;
            
            dwm_cascade_child_positions(dragged_window);
            
            dwm_invalidate_region(dragged_window->x - dragged_window->border_width, dragged_window->y - dragged_window->border_width, dragged_window->w + (dragged_window->border_width * 2), dragged_window->h + (dragged_window->border_width * 2));
            
            dragged_window->flags |= WINDOW_FLAG_REFRESH;
        }
    } else {
        // Released - drop the window
        dragged_window = NULL;
    }
}

void dwm_update_window_resizing(struct WindowContext* ctx) {
    if (resizing_window == NULL) return;
    if (resizing_window->frame_buffer == NULL) 
        return;
    
    if (ctx->left_button_pressed) {
        // Project desired sizing bounds derived from mouse tracking adjustments
        int target_w = (ctx->mouse.x + resize_offset_x) - resizing_window->x;
        int target_h = (ctx->mouse.y + resize_offset_y) - resizing_window->y;
        
        // Enforce a sensible minimum geometry threshold
        int min_width = 64;
        int min_height = 48 + resizing_window->titlebar_height;
        
        if (target_w < min_width)  target_w = min_width;
        if (target_h < min_height) target_h = min_height;
        
        // Enforce the maximum geometry threshold if defined (greater than 0)
        if (resizing_window->max_width > 0 && target_w > resizing_window->max_width) {
            target_w = resizing_window->max_width;
        }
        if (resizing_window->max_height > 0 && target_h > resizing_window->max_height) {
            target_h = resizing_window->max_height;
        }
        
        // Check if dimension shifts have actually happened
        if (resizing_window->w != target_w || resizing_window->h != target_h) {
            
            // Invalidate old window space configuration to clear out previous borders
            int border_ext = resizing_window->border_width;
            dwm_invalidate_region(resizing_window->x - border_ext, 
                                  resizing_window->y - border_ext, 
                                  resizing_window->w + (border_ext * 2), 
                                  resizing_window->h + (border_ext * 2));
            
            // Commit the new target geometry properties
            resizing_window->w = target_w;
            resizing_window->h = target_h;
            
            // Synchronize any nested hierarchical layouts attached to it
            dwm_sync_child_positions(resizing_window);
            
            // Flag the resize event so the window's callback receives it
            dwm_window_send_event(resizing_window->id, EVENT_RESIZE);
            
            // Request a complete paint pass and invalidate the updated boundaries
            resizing_window->flags |= (WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDRAW | WINDOW_FLAG_REDECORATE);
            dwm_invalidate_region(resizing_window->x - border_ext, 
                                  resizing_window->y - border_ext, 
                                  resizing_window->w + (border_ext * 2), 
                                  resizing_window->h + (border_ext * 2));
            
            // Re-compute client-surface mapping offsets
            resizing_window->surface_w = resizing_window->w;
            resizing_window->surface_h = resizing_window->h - resizing_window->titlebar_height;
            resizing_window->buffer_w  = target_w; 
            resizing_window->buffer_h  = target_h - resizing_window->titlebar_height;
            
            resizing_window->surface_x = resizing_window->x;
            resizing_window->surface_y = resizing_window->y + resizing_window->titlebar_height + (border_ext ? 1 : 0);
            
            // Safely reallocate internal window back-buffer surface dimensions
            uint32_t frame_buffer_sz = resizing_window->buffer_w * resizing_window->buffer_h * sizeof(uint32_t);
            if (resizing_window->frame_buffer != NULL) {
                free(resizing_window->frame_buffer);
            }
            resizing_window->frame_buffer = (uint32_t*)malloc(frame_buffer_sz);
            if (resizing_window->frame_buffer != NULL) {
                memset(resizing_window->frame_buffer, 0x11, frame_buffer_sz);
            }
            
            // Update the positioning coordinates of the resize handle button itself
            for (struct list_node* node = resizing_window->buttons_head; node != NULL; node = node->next) {
                struct WindowButton* btn = (struct WindowButton*)node->data;
                if (btn->event == EVENT_RESIZE) {
                    btn->x = resizing_window->w - btn->width;
                    btn->y = resizing_window->h - btn->height;
                }
                // Option: Update regular buttons (Close/Minimize) positions if window width changes
                if (btn->event == EVENT_CLOSE) {
                    btn->x = resizing_window->w - btn->width;
                }
                if (btn->event == EVENT_MINIMIZE) {
                    struct Image* button_close = dwm_resource_find("ui_close");
                    btn->x = resizing_window->w - button_close->width - btn->width;
                }
            }
            
            // Synchronize any nested hierarchical layouts attached to it
            dwm_sync_child_positions(resizing_window);
            
            // Request a complete paint pass and invalidate the updated boundaries
            resizing_window->flags |= (WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDRAW | WINDOW_FLAG_REDECORATE);
            dwm_invalidate_region(resizing_window->x - border_ext, 
                                  resizing_window->y - border_ext, 
                                  resizing_window->w + (border_ext * 2), 
                                  resizing_window->h + (border_ext * 2));
        }
    } else {
        // Drag click released - conclude the sizing operations safely
        resizing_window = NULL;
    }
}

void dwm_cascade_child_positions(struct WindowObject* parent) {
    if (parent == NULL) return;
    
    struct list_node* current = parent->children_head;
    while (current != NULL) {
        struct WindowObject* child = (struct WindowObject*)current->data;
        
        // Invalidate child's old screen space before shifting it
        dwm_invalidate_region(child->x - child->border_width, child->y - child->border_width,
                              child->w + (child->border_width * 2), child->h + (child->border_width * 2));
        
        // Push coordinate shifts relative to parent's newly calculated surface bounds
        child->x = parent->surface_x + child->local_x;
        child->y = parent->surface_y + child->local_y;
        
        child->surface_x = child->x;
        child->surface_y = child->y + child->titlebar_height + (child->border_width ? 1 : 0);
        
        // Mark child for refresh and invalidate its new screen space
        child->flags |= WINDOW_FLAG_REFRESH;
        dwm_invalidate_region(child->x - child->border_width, child->y - child->border_width,
                              child->w + (child->border_width * 2), child->h + (child->border_width * 2));
        
        // Recursively cycle through lower sub-menus/hierarchies if they exist
        dwm_cascade_child_positions(child);
        
        current = current->next;
    }
}
