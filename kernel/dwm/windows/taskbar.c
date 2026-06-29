#include <kernel/dwm/windows/taskbar.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/parser.h>

#include <kernel/vfs/vfs.h>
#include <kernel/events.h>

void callback_taskbar_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    switch (event) {
    case DWM_EVENT_MOUSE:
        break;
        
    case DWM_EVENT_REDRAW:
        dwm_draw_rect_filled_gradient(0, -3, display_get_width(), taskbar.height + 7, 0xFF000000, 0xFF00C000);
        break;
    }
}
