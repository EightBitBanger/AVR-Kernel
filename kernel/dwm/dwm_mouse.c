#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>

#include <kernel/console/display.h>
#include <kernel/dwm/icon_object.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>

#include <kernel/util/string.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

#include <kernel/dwm/dwm_core_internal.h>

bool handle_window_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click);
void handle_icon_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click);


void dwm_update_mouse(struct WindowContext* ctx) {
    bool is_new_left_click = ctx->left_button_pressed && !old_left_button_pressed;
    bool is_new_right_click = ctx->right_button_pressed && !old_right_button_pressed;
    
    if (!is_new_left_click && !is_new_right_click) return;
    
    if (context_menu.visible) {
        context_menu.visible = false;
        
        ctx->menu_moved = true;
        ctx->old_menu_min_x = context_menu.x;
        ctx->old_menu_min_y = context_menu.y;
        ctx->old_menu_max_x = context_menu.x + context_menu.w;
        ctx->old_menu_max_y = context_menu.y + context_menu.h;
    }
    
    if (handle_window_clicks(ctx, is_new_left_click, is_new_right_click)) 
        return;
    
    handle_icon_clicks(ctx, is_new_left_click, is_new_right_click);
}

bool handle_window_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click) {
    struct WindowObject* clicked_win = NULL;
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        int full_min_x = window->x - window->border_width;
        int full_max_x = window->x + window->w + window->border_width;
        int full_min_y = window->y - window->border_width;
        int full_max_y = window->y + window->h + window->border_width;
        
        if (ctx->mouse.x >= full_min_x && ctx->mouse.x <= full_max_x &&
            ctx->mouse.y >= full_min_y && ctx->mouse.y <= full_max_y) {
            clicked_win = window; 
        }
    }
    
    if (clicked_win == NULL) {
        return false; 
    }
    
    struct WindowObject* old_focused = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    
    dwm_set_focus(clicked_win);
    clicked_win->flags |= WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDRAW;
    
    if (old_focused && old_focused != clicked_win) {
        old_focused->flags |= WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDRAW;
        clicked_win->flags |= WINDOW_FLAG_REFRESH;
    }
    
    int title_min_x = clicked_win->x;
    int title_max_x = clicked_win->x + clicked_win->w;
    int title_min_y = clicked_win->y;
    int title_max_y = clicked_win->y + clicked_win->titlebar_height - 1;
    
    if (ctx->mouse.x >= title_min_x && ctx->mouse.x <= title_max_x &&
        ctx->mouse.y >= title_min_y && ctx->mouse.y <= title_max_y) {
        
        if (is_new_left_click) {
            dragged_window = clicked_win;
            drag_offset_x = ctx->mouse.x - clicked_win->x;
            drag_offset_y = ctx->mouse.y - clicked_win->y;
        } 
        else if (is_new_right_click) {
            
            // Right click on title bar
            
            //destroy_window((WindowHandle)clicked_win->id);
            
        }
    }
    
    return true; 
}

void handle_icon_clicks(struct WindowContext* ctx, bool is_new_left_click, bool is_new_right_click) {
    struct IconObject* clicked_icon = NULL;
    
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
            
            if (clicked_icon == last_clicked_icon && 
                (current_time - last_icon_click_time) <= DOUBLE_CLICK_THRESHOLD_MS) {
                
                //destroy_icon(clicked_icon);
                
                last_clicked_icon = NULL;
                last_icon_click_time = 0;
                
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
            
            // Context menu for file operations
            
            // TODO later this should be handled by a registry of sorts
            //      where file types can add associated context menu items
            
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
        }
    } else {
        if (is_new_left_click) {
            last_clicked_icon = NULL;
        }
        else if (is_new_right_click) {
            context_menu.visible = true;
            context_menu.x = ctx->mouse.x;
            context_menu.y = ctx->mouse.y;
            
            // Context menu for desktop operations
            
            // TODO same as with files a registry can provide menu action items
            
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
        }
    }
}
