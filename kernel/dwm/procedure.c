#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

void callback_button_close_handler(WindowHandle handle, wEvent event) {
    switch (event) {
    case EVENT_MOUSE:
        WindowHandle parent = dwm_window_get_parent(handle);
        dwm_window_send_event(parent, EVENT_CLOSE);
        break;
        
    case EVENT_REDRAW:
        dwm_draw_sprite(0, 0, dwm_resource_find("button_close"));
        break;
    }
}
