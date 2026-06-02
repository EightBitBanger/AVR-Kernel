#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>
#include <kernel/console/display.h>
#include <kernel/dwm/icon_object.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

#include <kernel/dwm/dwm_core_internal.h>

bool dwm_handle_window_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click);
void dwm_handle_icon_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click);

void dwm_update_mouse(struct WindowContext* ctx) {
    bool is_new_left_click = ctx->left_button_pressed && !old_left_button_pressed;
    bool is_new_right_click = ctx->right_button_pressed && !old_right_button_pressed;
    
    if (!is_new_left_click && !is_new_right_click) return;
    
    if (context_menu.visible) {
        context_menu.visible = false;
        dwm_invalidate_region(context_menu.x, context_menu.y, context_menu.w, context_menu.h);
    }
    
    if (dwm_handle_window_clicks(ctx, is_new_left_click, is_new_right_click)) 
        return;
    
    dwm_handle_icon_clicks(ctx, is_new_left_click, is_new_right_click);
}

struct WindowObject* dwm_find_clicked_window(struct list_node* tail_node, int mouse_x, int mouse_y) {
    // Traverse backwards through the current z-order level (top-most first)
    for (struct list_node* node = tail_node; node != NULL; node = node->prev) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        int abs_x, abs_y;
        dwm_get_absolute_position(window, &abs_x, &abs_y);
        
        int full_min_x = abs_x - window->border_width;
        int full_max_x = abs_x + window->w + window->border_width;
        int full_min_y = abs_y - window->border_width - window->titlebar_height;
        int full_max_y = abs_y + window->h + window->border_width;
        
        if (mouse_x >= full_min_x && mouse_x <= full_max_x &&
            mouse_y >= full_min_y && mouse_y <= full_max_y) {
            
            // Check child window clicks
            if (window->children_tail != NULL) {
                struct WindowObject* clicked_child = dwm_find_clicked_window(window->children_tail, mouse_x, mouse_y);
                if (clicked_child != NULL) {
                    return clicked_child; // A child intercepted the click!
                }
            }
            
            // No children where selected, this window itself is the target
            return window;
        }
    }
    return NULL;
}

bool dwm_handle_window_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click) {
    // Kick off the recursive search starting from the top-most desktop window
    struct WindowObject* clicked_win = dwm_find_clicked_window(window_tail, ctx->mouse.x, ctx->mouse.y);
    
    if (clicked_win == NULL) return false;
    
    // Assign the mouse event to the specific window or child button that was clicked
    clicked_win->events |= EVENT_MOUSE;
    
    // If it's a child window, we want to shift focus/z-order tracking to its top-level root parent
    struct WindowObject* root_win = clicked_win;
    while (root_win->parent != NULL) {
        root_win = root_win->parent;
    }
    
    struct WindowObject* old_focused = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    
    if (old_focused != root_win) {
        dwm_set_focus(root_win);
        root_win->flags |= (WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE); 
        
        if (old_focused) {
            old_focused->flags |= (WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE);
            
            int old_abs_x, old_abs_y;
            dwm_get_absolute_position(old_focused, &old_abs_x, &old_abs_y);
            dwm_invalidate_region(old_abs_x - old_focused->border_width, 
                                  old_abs_y - old_focused->border_width, 
                                  old_focused->w + (old_focused->border_width * 2), 
                                  old_focused->h + (old_focused->border_width * 2));
        }
        
        int root_abs_x, root_abs_y;
        dwm_get_absolute_position(root_win, &root_abs_x, &root_abs_y);
        dwm_invalidate_region(root_abs_x - root_win->border_width, 
                              root_abs_y - root_win->border_width, 
                              root_win->w + (root_win->border_width * 2), 
                              root_win->h + (root_win->border_width * 2));
    }
    
    int clicked_abs_x, clicked_abs_y;
    dwm_get_absolute_position(clicked_win, &clicked_abs_x, &clicked_abs_y);
    
    int title_min_x = clicked_abs_x;
    int title_max_x = clicked_abs_x + clicked_win->w;
    int title_min_y = clicked_abs_y;
    int title_max_y = clicked_abs_y + clicked_win->titlebar_height - 1;
    
    if ((!(clicked_win->style & WINDOW_STYLE_NOBORDERS)) && 
        ctx->mouse.x >= title_min_x && ctx->mouse.x <= title_max_x && 
        ctx->mouse.y >= title_min_y && ctx->mouse.y <= title_max_y) {
        
        drag_offset_x = ctx->mouse.x - clicked_win->x;
        drag_offset_y = ctx->mouse.y - clicked_win->y;
        
        if (is_new_left_click) {
            dragged_window = clicked_win;
        }
    }
    
    return true; 
}

void dwm_handle_icon_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click) {
    struct IconObject* clicked_icon = NULL;
    
    // Get a clicked icon
    
    for (struct list_node* node = icon_head; node != NULL; node = node->next) {
        struct IconObject* icon = (struct IconObject*)node->data;
        int icon_min_x = icon->x + icon->bounds_x;
        int icon_max_x = icon->x + icon->bounds_x + icon->bounds_w;
        int icon_min_y = icon->y + icon->bounds_y;
        int icon_max_y = icon->y + icon->bounds_y + icon->bounds_h;
        
        if (ctx->mouse.x >= icon_min_x && ctx->mouse.x <= icon_max_x &&
            ctx->mouse.y >= icon_min_y && ctx->mouse.y <= icon_max_y) {
            clicked_icon = icon;
            break; 
        }
    }
    
    if (clicked_icon != NULL) {
        if (is_new_left_click) {
            uint32_t current_time = timer_get_ms();
            if (clicked_icon == last_clicked_icon && (current_time - last_icon_click_time) <= DOUBLE_CLICK_THRESHOLD_MS) {
                last_clicked_icon = NULL;
                last_icon_click_time = 0;
                
                // Left mouse double click
                
                WindowClass wclass;
                wclass.x = 200;
                wclass.y = 100;
                wclass.width = 250;
                wclass.height = 250;
                
                WindowHandle window = create_window(wclass, 0, NULL);
                
            } else {
                dragged_icon = clicked_icon;
                
                icon_drag_offset_x = ctx->mouse.x - clicked_icon->x;
                icon_drag_offset_y = ctx->mouse.y - clicked_icon->y;
                last_clicked_icon = clicked_icon;
                last_icon_click_time = current_time;
            }
        }
        else if (is_new_right_click) {
            context_menu.visible = true;
            context_menu.x = ctx->mouse.x;
            context_menu.y = ctx->mouse.y;
            context_menu.item_height = 22;
            context_menu.item_count = 4;
            context_menu.w = 130;
            context_menu.h = context_menu.item_height * context_menu.item_count;
            
            strcpy(context_menu.item[0].name, "Open");
            strcpy(context_menu.item[1].name, "Copy");
            strcpy(context_menu.item[2].name, "Delete");
            strcpy(context_menu.item[3].name, "Properties");
            
            int display_w = display_get_width();
            int display_h = display_get_height();
            if (context_menu.x + context_menu.w > display_w) context_menu.x = display_w - context_menu.w;
            if (context_menu.y + context_menu.h > display_h - taskbar_height) context_menu.y = display_h - taskbar_height - context_menu.h;
            
            dwm_invalidate_region(context_menu.x, context_menu.y, context_menu.w, context_menu.h);
        }
    } else {
        
        // Desktop click
        
        if (is_new_left_click) {
            
            last_clicked_icon = NULL;
        }
        else if (is_new_right_click) {
            context_menu.visible = true;
            context_menu.x = ctx->mouse.x;
            context_menu.y = ctx->mouse.y;
            context_menu.item_height = 22;
            context_menu.item_count  = 2;
            context_menu.w = 120;
            context_menu.h = context_menu.item_height * context_menu.item_count;
            
            strcpy(context_menu.item[0].name, "New");
            strcpy(context_menu.item[1].name, "Properties");
            
            int display_w = display_get_width();
            int display_h = display_get_height();
            if (context_menu.x + context_menu.w > display_w) context_menu.x = display_w - context_menu.w;
            if (context_menu.y + context_menu.h > display_h - taskbar_height) context_menu.y = display_h - taskbar_height - context_menu.h;
            
            dwm_invalidate_region(context_menu.x, context_menu.y, context_menu.w, context_menu.h);
        }
    }
}
