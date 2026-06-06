#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

void dwm_process_window_events(struct WindowObject* window) {
    if (window == NULL) return;
    
    // Get current mouse state from global context
    int mx = window_context.mouse.x;
    int my = window_context.mouse.y;
    
    // Calculate the actual absolute boundaries of this window
    int win_x = window->x;
    int win_y = window->y;
    int win_w = window->w;
    int win_h = window->h;
    
    // If it's a child window, clamp its active event zone to the parent's surface
    if (window->parent != NULL) {
        int px = window->parent->surface_x;
        int py = window->parent->surface_y;
        int pw = window->parent->surface_w;
        int ph = window->parent->surface_h;
        
        // Intersect window bounds with parent surface bounds
        int ix1 = (win_x > px) ? win_x : px;
        int iy1 = (win_y > py) ? win_y : py;
        int ix2 = (win_x + win_w < px + pw) ? win_x + win_w : px + pw;
        int iy2 = (win_y + win_h < py + ph) ? win_y + win_h : py + ph;
        
        // If the child is completely pushed outside the parent, it has no active event zone
        if (ix1 >= ix2 || iy1 >= iy2) {
            return; // Block all events! Mouse cannot possibly click it safely.
        }
        
        // Update our hit-test bounds to the clipped intersection rectangle
        win_x = ix1;
        win_y = iy1;
        win_w = ix2 - ix1;
        win_h = iy2 - iy1;
    }
    
    // Perform standard hit-testing against the updated (potentially shrunk) bounds
    bool mouse_inside = (mx >= win_x && mx < win_x + win_w && 
                        my >= win_y && my < win_y + win_h);
    
    if (!mouse_inside) 
        return; // Mouse is outside the visible/clamped area of the button
    
    if (window->event_callback != NULL && window->events != 0) {
        if (window->events & EVENT_MOUSE)     {window->events &= ~EVENT_MOUSE;     window->event_callback(window->id, EVENT_MOUSE);}
        if (window->events & EVENT_KEYBOARD)  {window->events &= ~EVENT_KEYBOARD;  window->event_callback(window->id, EVENT_KEYBOARD);}
        
        if (window->events & EVENT_REDRAW)    {window->events &= ~EVENT_REDRAW;
            dwm_invalidate_region(window->x, window->y, window->w, window->h);
            window->flags |= (WINDOW_FLAG_REDRAW | WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE);
        }
    }
}

void dwm_process_context_menu_events(uint16_t index) {
    
    switch (context_menu_directive) {
    case CONTEXT_MENU_DESKTOP:
        extern void trigger_test_page_fault(void);
        //if (index == 0) 
        
        break;
        
    case CONTEXT_MENU_ICON:
        
        
        break;
    }
    
}
