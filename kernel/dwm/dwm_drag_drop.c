#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>

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
