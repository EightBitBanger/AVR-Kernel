#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/events.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
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


void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h) {
    dwm_invalidate_region(x, y, w, h);
}

void dwm_invalidate_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w <= 0 || h <= 0) return;
    
    // Bounds clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    
    int display_w = display_get_width();
    int display_h = display_get_height();
    if (x + w > display_w) w = display_w - x;
    if (y + h > display_h) h = display_h - y;
    if (w <= 0 || h <= 0) return;
    
    if (window_context.dirty_count < MAX_DIRTY_RECTS) {
        window_context.dirty_regions[window_context.dirty_count].x = x;
        window_context.dirty_regions[window_context.dirty_count].y = y;
        window_context.dirty_regions[window_context.dirty_count].w = w;
        window_context.dirty_regions[window_context.dirty_count].h = h;
        window_context.dirty_count++;
    } else {
        
        int min_x = window_context.dirty_regions[0].x;
        int min_y = window_context.dirty_regions[0].y;
        int max_x = min_x + window_context.dirty_regions[0].w;
        int max_y = min_y + window_context.dirty_regions[0].h;
        
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x + w > max_x) max_x = x + w;
        if (y + h > max_y) max_y = y + h;
        
        window_context.dirty_regions[0].x = min_x;
        window_context.dirty_regions[0].y = min_y;
        window_context.dirty_regions[0].w = max_x - min_x;
        window_context.dirty_regions[0].h = max_y - min_y;
        window_context.dirty_count = 1;
    }
}

void dwm_calculate_icon_bounds(struct IconObject* icon) {
    size_t length = strlen(icon->name);
    int16_t text_width = length * 6;
    int16_t text_height = 8;        
    int16_t icon_text_height = 45;  
    
    if (text_width > icon->width) {
        icon->bounds_x = (icon->width - text_width) / 2;
        icon->bounds_w = text_width;
    } else {
        icon->bounds_x = 0;
        icon->bounds_w = icon->width;
    }
    
    icon->bounds_y = 0;
    icon->bounds_h = icon_text_height + text_height;
}

void dwm_get_absolute_position(struct WindowObject* window, int* out_x, int* out_y) {
    if (window->parent == NULL) {
        *out_x = window->x;
        *out_y = window->y;
        return;
    }
    
    int abs_x = window->local_x;
    int abs_y = window->local_y;
    
    struct WindowObject* p = window->parent;
    while (p != NULL) {
        abs_x += p->surface_x; 
        abs_y += p->surface_y;
        p = p->parent;
    }
    
    *out_x = abs_x;
    *out_y = abs_y;
}
