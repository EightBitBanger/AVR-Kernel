#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

void dwm_process_events(struct WindowObject* window) {
    
    if (window->event_callback != NULL && window->events != 0) {
        if (window->events & EVENT_MOUSE)     {window->events &= ~EVENT_MOUSE;     window->event_callback(window->id, EVENT_MOUSE);}
        if (window->events & EVENT_KEYBOARD)  {window->events &= ~EVENT_KEYBOARD;  window->event_callback(window->id, EVENT_KEYBOARD);}
        
        if (window->events & EVENT_REDRAW)    {window->events &= ~EVENT_REDRAW;
            dwm_invalidate_region(window->x, window->y, window->w, window->h);
            window->flags |= (WINDOW_FLAG_REDRAW | WINDOW_FLAG_REFRESH | WINDOW_FLAG_REDECORATE);
        }
    }
}
