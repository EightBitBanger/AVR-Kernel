#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

void dwm_process_window_events(struct WindowObject* window) {
    if (window == NULL) return;
    
    // Keep global mouse coordinates for the hit-test bounds check
    int mouse_world_x = window_context.mouse.x;
    int mouse_world_y = window_context.mouse.y;
    
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
        
        // Update our hit-test bounds to the clipped intersection rectangle
        win_x = ix1;
        win_y = iy1;
        win_w = ix2 - ix1;
        win_h = iy2 - iy1;
    }
    
    // Check mouse is inside the visible/clamped area using absolute screen space coordinates
    if (!(mouse_world_x >= win_x && mouse_world_x < win_x + win_w && 
          mouse_world_y >= win_y && mouse_world_y < win_y + win_h)) 
        return;
    
    // Hit-test passed! Now convert to surface-local coordinates for user packing
    int mx = mouse_world_x - window->surface_x;
    int my = mouse_world_y - window->surface_y;
    
    // Lower limit
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    
    // Pack data down for the callback function
    uint32_t window_sz_data = ((uint32_t)(uint16_t)window->h << 16) | ((uint32_t)(uint16_t)window->w & 0xFFFF);
    
    uint32_t mouse_data = ((uint32_t)(uint16_t)my << 16) | ((uint32_t)(uint16_t)mx & 0xFFFF);
    int32_t mouse_state = 0;
    if (window_context.left_button_pressed)  mouse_state |= DWM_STATE_MOUSE_BTN_LEFT;
    if (window_context.right_button_pressed) mouse_state |= DWM_STATE_MOUSE_BTN_RIGHT;
    
    // On double click event (UPDATED)
    if (window_context.is_double_click) {
        mouse_state |= DWM_STATE_MOUSE_DOUBLE_CLK;
    }
    
    if (window->event_callback != NULL && window->events != 0) {
        if (window->events & DWM_EVENT_MOUSE)     {window->events &= ~DWM_EVENT_MOUSE;     window->event_callback(window->id, DWM_EVENT_MOUSE, mouse_data, mouse_state);}
        if (window->events & DWM_EVENT_KEYBOARD)  {window->events &= ~DWM_EVENT_KEYBOARD;  window->event_callback(window->id, DWM_EVENT_KEYBOARD, 0, 0);}
        
        if (window->events & DWM_EVENT_RESIZE)    {window->events &= ~DWM_EVENT_RESIZE;    window->event_callback(window->id, DWM_EVENT_RESIZE, window_sz_data, 0);}
        
        if (window->events & DWM_EVENT_REDRAW)    {window->events &= ~DWM_EVENT_REDRAW;
            dwm_invalidate_region(window->x, window->y, window->w, window->h);
            window->flags |= (DWM_WFLAG_REDRAW | DWM_WFLAG_REFRESH | DWM_WFLAG_REDECORATE);
        }
    }
}

void dwm_process_context_menu_events(struct WindowContext* ctx, uint16_t index) {
    
    switch (context_menu_directive) {
        
    case DWM_CONTEXT_MENU_DESKTOP:
        extern void trigger_test_page_fault(void);
        
        switch (index) {
        case 0:
            
            //dwm_create_folder(ctx->mouse.x, ctx->mouse.y, "system");
            
            dwm_summon_message_box("window title", "Error message");
            
            break;
        }
        
        break;
        
    case DWM_CONTEXT_MENU_ICON:
        
        switch (index) {
        case 3:
            
            dwm_summon_properties("Properties", "/mnt");
            
            break;
        }
        
        break;
    }
    
}
