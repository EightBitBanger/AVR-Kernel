#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/util/timer.h>
#include <kernel/dwm/dwm_dispatch.h>

#define DWM_MSG_QUEUE_SIZE 256

static DWMMessage msg_queue[DWM_MSG_QUEUE_SIZE];
static int msg_head = 0;
static int msg_tail = 0;

bool dwm_post_message(WindowHandle hwnd, wEvent message, uint32_t wparam, int32_t lparam) {
    mutex_lock(&dwm_mutex);

    int next_tail = (msg_tail + 1) % DWM_MSG_QUEUE_SIZE;
    if (next_tail == msg_head) {
        mutex_unlock(&dwm_mutex);
        return false; // Queue overflow
    }

    msg_queue[msg_tail].hwnd    = hwnd;
    msg_queue[msg_tail].message = message;
    msg_queue[msg_tail].wparam  = wparam;
    msg_queue[msg_tail].lparam  = lparam;
    msg_queue[msg_tail].time    = timer_get_ms(); 

    msg_tail = next_tail;

    mutex_unlock(&dwm_mutex);
    return true;
}

bool dwm_get_message(DWMMessage* out_msg) {
    mutex_lock(&dwm_mutex);

    if (msg_head == msg_tail) {
        mutex_unlock(&dwm_mutex);
        return false;
    }

    *out_msg = msg_queue[msg_head];
    msg_head = (msg_head + 1) % DWM_MSG_QUEUE_SIZE;

    mutex_unlock(&dwm_mutex);
    return true;
}

void dwm_dispatch_message(const DWMMessage* msg) {
    mutex_lock(&dwm_mutex);
    struct WindowObject* window = dwm_get_window_by_id(msg->hwnd);
    if (window == NULL || window->event_callback == NULL) {
        mutex_unlock(&dwm_mutex);
        return;
    }

    WindowProcedure callback = window->event_callback;
    WindowHandle win_id = window->id;

    // Handle built-in window mechanics
    if (msg->message == DWM_EVENT_REDRAW) {
        int abs_x, abs_y;
        dwm_get_absolute_position(window, &abs_x, &abs_y);

        // Invalidate region including borders
        dwm_invalidate_region(abs_x - window->border_width, 
                              abs_y - window->border_width, 
                              window->w + (window->border_width * 2), 
                              window->h + (window->border_width * 2));

        window->flags |= (DWM_WFLAG_REDRAW | DWM_WFLAG_REFRESH | DWM_WFLAG_REDECORATE);
    }
    mutex_unlock(&dwm_mutex);

    // Call window user callback OUTSIDE the mutex lock to avoid deadlocks
    callback(win_id, msg->message, msg->wparam, msg->lparam);

    // Clean up window automatically if CLOSE message was dispatched
    if (msg->message == DWM_EVENT_CLOSE) {
        callback(win_id, DWM_EVENT_DESTROY, 0, 0);
        dwm_destroy_window(win_id);
    }
}
