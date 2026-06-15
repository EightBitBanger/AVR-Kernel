#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/console/display.h>

void callback_button_close_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
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

void callback_taskbar_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    switch (event) {
    case EVENT_MOUSE:
        break;
        
    case EVENT_REDRAW:
        dwm_draw_rect_filled_gradient(0, -3, display_get_width(), taskbar_height + 7, 0xFF000000, 0xFF00C000);
        break;
    }
}
