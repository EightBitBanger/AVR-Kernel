#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/util/timer.h>
#include <kernel/dwm/dwm_dispatch.h>

#define DWM_MSG_QUEUE_SIZE 256

static DWMMessage msg_queue[DWM_MSG_QUEUE_SIZE];
static int msg_head = 0;
static int msg_tail = 0;

bool dwm_post_message(WindowHandle hwnd, wEvent message, uint32_t wparam, int32_t lparam) {
    int next_tail = (msg_tail + 1) % DWM_MSG_QUEUE_SIZE;
    if (next_tail == msg_head) 
        return false; // Queue overflow
    
    msg_queue[msg_tail].hwnd    = hwnd;
    msg_queue[msg_tail].message = message;
    msg_queue[msg_tail].wparam  = wparam;
    msg_queue[msg_tail].lparam  = lparam;
    msg_queue[msg_tail].time    = timer_get_ms(); 
    
    msg_tail = next_tail;
    return true;
}

bool dwm_get_message(DWMMessage* out_msg) {
    if (msg_head == msg_tail) 
        return false;
    
    *out_msg = msg_queue[msg_head];
    msg_head = (msg_head + 1) % DWM_MSG_QUEUE_SIZE;
    
    return true;
}

void dwm_dispatch_message(const DWMMessage* msg) {
    struct WindowObject* window = dwm_get_window_by_id(msg->hwnd);
    if (window == NULL || window->event_callback == NULL) return;

    // Handle built-in window mechanics before or alongside callback invocation
    if (msg->message == DWM_EVENT_REDRAW) {
        dwm_invalidate_region(window->x, window->y, window->w, window->h);
        window->flags |= (DWM_WFLAG_REDRAW | DWM_WFLAG_REFRESH | DWM_WFLAG_REDECORATE);
    }

    // Execute window user callback
    window->event_callback(window->id, msg->message, msg->wparam, msg->lparam);

    // Clean up window automatically if CLOSE message was dispatched
    if (msg->message == DWM_EVENT_CLOSE) {
        window->event_callback(window->id, DWM_EVENT_DESTROY, 0, 0);
        dwm_destroy_window(window->id);
    }
}
