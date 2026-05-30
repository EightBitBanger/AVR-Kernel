#include <kernel/dwm/dwm.h>
#include <kernel/dwm/icons.h>

#include <kernel/console/display.h>
#include <kernel/dwm/icon_object.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>

#include <kernel/util/string.h>

#include <kernel/dwm/dwm_core_internal.h>

void dwm_draw_desktop(const struct WindowContext* ctx) {
    
    // Invalidate old screen areas
    
    draw_rect_filled(mouse_old.x, mouse_old.y, ctx->cursor_width, ctx->cursor_height, bg_color);
    
    if (ctx->window_moved) {
        int16_t win_w = ctx->old_win_max_x - ctx->old_win_min_x;
        int16_t win_h = ctx->old_win_max_y - ctx->old_win_min_y;
        draw_rect_filled(ctx->old_win_min_x, ctx->old_win_min_y, win_w, win_h, bg_color);
    }
    
    if (ctx->icon_moved) {
        int16_t icon_w = ctx->old_icon_max_x - ctx->old_icon_min_x;
        int16_t icon_h = ctx->old_icon_max_y - ctx->old_icon_min_y;
        draw_rect_filled(ctx->old_icon_min_x, ctx->old_icon_min_y, icon_w, icon_h, bg_color);
    }
    
    if (ctx->menu_moved) {
        int16_t menu_w = ctx->old_menu_max_x - ctx->old_menu_min_x;
        int16_t menu_h = ctx->old_menu_max_y - ctx->old_menu_min_y;
        draw_rect_filled(ctx->old_menu_min_x, ctx->old_menu_min_y, menu_w, menu_h, bg_color);
    }
    
    // Taskbar Setup Bounds
    
    uint16_t bar_x = 0;
    uint16_t bar_y = display_get_height() - taskbar_height;
    uint16_t bar_w = display_get_width();
    uint16_t bar_h = taskbar_height;
    
    // Draw over old taskbar
    if (ctx->max_y >= bar_y) 
        draw_rect_filled(bar_x, bar_y, bar_w, bar_h, bg_color);
    
    //
    // Draw Desktop Icons
    
    struct list_node* current_node = icon_head;
    while (current_node != NULL) {
        struct IconObject* current_icon = (struct IconObject*)current_node->data;
        
        // Use the proper relative bounds positions for dirty pixel evaluations
        bool separate_x = (current_icon->x + current_icon->bounds_x >= ctx->max_x) || 
                          ((current_icon->x + current_icon->bounds_x + current_icon->bounds_w) <= ctx->min_x);
        bool separate_y = (current_icon->y + current_icon->bounds_y >= ctx->max_y) || 
                          ((current_icon->y + current_icon->bounds_y + current_icon->bounds_h) <= ctx->min_y);
        bool in_refresh_zone = !(separate_x || separate_y);
        
        if (in_refresh_zone || ctx->window_moved || ctx->icon_moved || ctx->menu_moved) {
            
            // Draw icon sprite image
            
            if (current_icon->icon_sprite != NULL) {
                draw_sprite_blend(current_icon->icon_sprite, current_icon->width, current_icon->height, current_icon->x, current_icon->y, 0xFF000000);
            } else {
                draw_rect_filled(current_icon->x, current_icon->y, current_icon->width, current_icon->height, 0xFFFFFFFF);
            }
            
            // Draw name text
            
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
    
    //
    // Draw windows
    
    extern struct ClippingPlane clipping_plain;
    extern struct MultibootInfo* vinfo;
    extern uint32_t* front_buffer;
    extern uint32_t* back_buffer;
    extern uint32_t* frame_buffer;
    const uint32_t screen_stride = vinfo->framebuffer_pitch / 4;
    
    for (struct list_node* node = window_head; node != NULL; node = node->next) {
        struct WindowObject* window = (struct WindowObject*)node->data;
        
        int win_min_x = window->x - window->border_width;
        int win_min_y = window->y - window->border_width;
        int win_max_x = window->x + window->w + window->border_width;
        int win_max_y = window->y + window->h + window->border_width;
        
        int main_redraw = !((win_min_x >= ctx->max_x) || (win_max_x <= ctx->min_x) ||
                            (win_min_y >= ctx->max_y) || (win_max_y <= ctx->min_y));
        
        int mouse_redraw = !((win_min_x >= (mouse_old.x + ctx->cursor_width)) || (win_max_x <= mouse_old.x) ||
                            (win_min_y >= (mouse_old.y + ctx->cursor_height)) || (win_max_y <= mouse_old.y));
        
        int do_redraw = main_redraw || mouse_redraw || (window->flags & (WINDOW_FLAG_REDRAW | WINDOW_FLAG_REFRESH)) || ctx->menu_moved;
        
        if (!do_redraw) 
            continue;
        
        if (window->flags & WINDOW_FLAG_REDRAW) {
            window->flags &= ~WINDOW_FLAG_REDRAW;
            
            draw_set_clip_rect(0, 0, window->buffer_w, window->buffer_h);
            draw_set_buffer(window->frame_buffer, window->buffer_w, window->buffer_h);
            
            event_window = window;
            if (window->event_callback != NULL) {
                window->event_callback(window->id, EVENT_REDRAW);
            }
            event_window = NULL;
            
            draw_set_clip_rect(0, 0, display_get_width(), display_get_height());
            draw_set_buffer_default();
        }
        
        if ((window->flags & WINDOW_FLAG_REFRESH) || mouse_redraw || main_redraw) {
            window->flags &= ~WINDOW_FLAG_REFRESH;
            
            int clip_x = win_min_x + 1;
            int clip_y = win_min_y + window->titlebar_height + 1;
            int clip_w = win_max_x - win_min_x - 2;
            int clip_h = win_max_y - win_min_y - window->titlebar_height - 2;
            
            dwm_upload_window_buffer_to_backbuffer(window, frame_buffer, screen_stride, clip_x, clip_y, clip_w, clip_h);
            
            dwm_draw_window(window);
            
        }
    }
    
    //
    // Draw Taskbar
    
    if (ctx->max_y >= bar_y) {
        
        // TODO get this outta heeeere
        //      maybe not implement a taskbar.. considering options
        
        uint32_t bar_color_low  = 0x7F008000;
        uint32_t bar_color_high = 0x7F001000;
        
        //draw_rect_gradient_vertical(bar_x, bar_y, bar_w, bar_h, bar_color_low, bar_color_high);
    }
    
    //
    // Render desktop context menu
    
    if (context_menu.visible) {
        uint16_t menu_height = context_menu.item_height * context_menu.item_count;
        context_menu.h = menu_height;
        
        draw_rect_filled(context_menu.x, context_menu.y, 
                         context_menu.w, menu_height, context_menu.color_bg);
        draw_rect(context_menu.x, context_menu.y, 
                  context_menu.w, menu_height, context_menu.color_border);
        
        for (unsigned int i=0; i < context_menu.item_count; i++) {
            uint16_t intersector_height = context_menu.item_height * i;
            
            uint16_t menu_x = context_menu.x + 1;
            uint16_t menu_y = context_menu.y + intersector_height;
            uint16_t menu_w = context_menu.w - 2;
            uint16_t menu_h = context_menu.item_height;
            
            uint16_t text_x = menu_x + 3;
            uint16_t text_y = menu_y + 7;
            
            if ((int)i == context_menu.hovered_item) {
                draw_rect_filled(menu_x, menu_y + 1, menu_w, menu_h - 1, context_menu.color_highlight);
            }
            
            if (i > 0 && (int)i != context_menu.hovered_item && (int)(i - 1) != context_menu.hovered_item) {
                draw_line(menu_x, menu_y, menu_x + menu_w, menu_y, context_menu.color_separator);
            }
            
            size_t length = strlen(context_menu.item[i].name);
            for (unsigned int s=0; s < length; s++) {
                draw_glyph(char_rom, context_menu.item[i].name[s], text_x + (s * 6), text_y, context_menu.color_text, 0xFF000000, 0xFF000000);
            }
        }
    }
    
    // Redraw the cursor on the top layer
    draw_sprite_blend(cursor.data, ctx->cursor_width, ctx->cursor_height, ctx->mouse.x, ctx->mouse.y, 0xFF000000);
}

void dwm_draw_window(struct WindowObject* window_handle) {
    int32_t wx = window_handle->x;
    int32_t wy = window_handle->y;
    int32_t ww = window_handle->w;
    int32_t wh = window_handle->h;
    
    /*
    struct WindowObject* focused_window = (window_tail != NULL) ? (struct WindowObject*)window_tail->data : NULL;
    
    uint32_t current_title_color_low = (window_handle == focused_window) ? 
                                       window_handle->title_color_low : 
                                       window_handle->inactive_color_low;
    
    uint32_t current_title_color_high = (window_handle == focused_window) ? 
                                        window_handle->title_color_high : 
                                        window_handle->inactive_color_high;
    */
    
    /*
    if (window_handle->titlebar_height > 0) {
        draw_rect_gradient_vertical(wx, wy, ww, 
                                    window_handle->titlebar_height, 
                                    current_title_color_low, 
                                    current_title_color_high);
    }
    */
    for (uint8_t b = 1; b <= window_handle->border_width; b++) {
        draw_rect(
            window_handle->x - b, 
            window_handle->y - b, 
            window_handle->w + (b * 2), 
            window_handle->h + (b * 2), 
            window_handle->border_color
        );
    }
    
    for (uint8_t b = 0; b < window_handle->border_width; b++) {
        draw_line(
            window_handle->x, 
            window_handle->y + window_handle->titlebar_height + b - 1, 
            window_handle->x + window_handle->w, 
            window_handle->y + window_handle->titlebar_height + b - 1, 
            window_handle->border_color
        );
    }
    
    /*
    draw_sprite(
        button_x.data, button_x.width, button_x.height, 
        window_handle->x + window_handle->w - 15, 
        window_handle->y + (7 - (window_handle->border_width / 2) - 1), 
        0xFF000000
    );
    */
}

