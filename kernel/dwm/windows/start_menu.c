
#include <kernel/dwm/windows/start_menu.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/util/string.h>
#include <kernel/vfs/vfs.h>
#include <kernel/registry/registry.h>
#include <kernel/events.h>

// Single instance; 0 while closed
static WindowHandle start_menu_handle = 0;

// ==========================================
// Layout (window-local coordinates)
// ==========================================
#define SM_HEADER_H        44
#define SM_ITEM_TOP        50
#define SM_ROW_H           24
#define SM_ROW_X           6
#define SM_ACCENT_W        4
#define SM_BULLET_SIZE     8
#define SM_TEXT_X          30

// Colors (from the system theme)
#define SM_COLOR_BG          (theme.client.background)
#define SM_COLOR_ACCENT      (theme.client.accent)
#define SM_COLOR_DIVIDER     (theme.start_menu.separator)
#define SM_COLOR_TEXT        (theme.client.text)
#define SM_COLOR_SUBTEXT     (theme.client.text_muted)
#define SM_COLOR_ROW         (theme.start_menu.row)
#define SM_COLOR_ROW_BORDER  (theme.start_menu.row_border)

// ==========================================
// Menu items
// ==========================================
// Bullet colors identify each item, so they stay here rather than in the theme
typedef void (*StartMenuAction)(void);

typedef struct {
    const char*     text;
    uint32_t        bullet_color;
    StartMenuAction action;
} StartMenuItem;

static void action_explorer(void) {
    kernel_event_send(KEVENT_EXECUTE, "explorer", "/");
}

static void action_documents(void) {
    struct LocalPaths paths;
    kernel_get_local_paths(&paths);
    kernel_event_send(KEVENT_EXECUTE, "explorer", paths.home);
}

static void action_notepad(void) {
    kernel_event_send(KEVENT_EXECUTE, "notepad", "");
}

static void action_settings(void) {
    dwm_summon_message_box("Settings", "Settings are not implemented yet");
}

static void action_run(void) {
    dwm_summon_message_box("Run", "Run dialog is not implemented yet");
}

static void action_shutdown(void) {
    // Syncing mid-copy would save a half-written file
    if (dwm_filecopy_busy()) {
        dwm_summon_message_box("Shut down", "Wait for the copy to finish (or cancel it) first");
        return;
    }
    
    // Everything that writes to disk goes first; the device flush goes last
    // so it pushes out what the earlier steps left in the caches. Every step
    // runs even if an earlier one fails, so one problem doesn't block the rest.
    bool layout_ok   = dwm_desktop_layout_save();
    bool registry_ok = registry_hive_save_all();
    bool storage_ok  = vfs_sync_all();
    
    if (layout_ok && registry_ok && storage_ok) {
        dwm_summon_message_box("Shut down", "Saved. It is safe to power off");
        return;
    }
    
    // Name what failed (fits the 300px message box at 6px per glyph)
    char message[64] = "Save failed:";
    if (!layout_ok)   strncat(message, " desktop", sizeof(message) - strlen(message) - 1);
    if (!registry_ok) strncat(message, " registry", sizeof(message) - strlen(message) - 1);
    if (!storage_ok)  strncat(message, " storage", sizeof(message) - strlen(message) - 1);
    
    dwm_summon_message_box("Shut down", message);
}

static const StartMenuItem main_items[] = {
    { "File explorer", 0xFFFDA008, action_explorer  },
    { "Documents",     0xFF3F9FFF, action_documents },
    { "Notepad",       0xFF3FFF3F, action_notepad   },
    { "Settings",      0xFFAAAAAA, action_settings  },
    { "Run...",        0xFFF700F7, action_run       },
};

static const StartMenuItem footer_item = { "Shut down", 0xFFC00404, action_shutdown };

#define SM_MAIN_COUNT  (sizeof(main_items) / sizeof(main_items[0]))

// Footer row sits below the main list plus a separator gap
static uint16_t footer_row_y(void) {
    return SM_ITEM_TOP + (SM_MAIN_COUNT * SM_ROW_H) + 12;
}

bool dwm_start_menu_is_open(void) {
    if (start_menu_handle == 0) return false;
    if (dwm_get_window_by_id(start_menu_handle) == NULL) {
        start_menu_handle = 0;
        return false;
    }
    return true;
}

WindowHandle dwm_start_menu_open(void) {
    if (dwm_start_menu_is_open()) return start_menu_handle;

    WindowClass wclass;
    memset(&wclass, 0, sizeof(WindowClass));

    wclass.width      = DWM_START_MENU_WIDTH;
    wclass.height     = DWM_START_MENU_HEIGHT;
    wclass.max_width  = DWM_START_MENU_WIDTH;
    wclass.max_height = DWM_START_MENU_HEIGHT;

    // Bottom left, sitting just above the taskbar (1px border + 2px gap)
    wclass.x = 3;
    wclass.y = display_get_height() - taskbar.height - DWM_START_MENU_HEIGHT - 3;

    strncpy(wclass.title, "Start", DWM_MAX_TITLE_LEN - 1);

    struct WindowObject* menu = dwm_allocate_window(
        wclass,
        DWM_WSTYLE_TOPMOST | DWM_WSTYLE_NOTITLEBAR | DWM_WSTYLE_NOCLOSEBOX,
        (WindowProcedure)callback_start_menu_handler
    );
    if (menu == NULL) return 0;

    start_menu_handle = menu->id;
    dwm_set_focus(menu);

    // Let the taskbar draw its button pressed
    dwm_window_send_event(taskbar.window, DWM_EVENT_REDRAW);
    return start_menu_handle;
}

void dwm_start_menu_close(void) {
    if (!dwm_start_menu_is_open()) return;
    dwm_window_send_event(start_menu_handle, DWM_EVENT_CLOSE);
}

void dwm_start_menu_toggle(void) {
    if (dwm_start_menu_is_open()) {
        dwm_start_menu_close();
    } else {
        dwm_start_menu_open();
    }
}

// ==========================================
// Drawing
// ==========================================
static void draw_row(uint16_t width, uint16_t row_y, const StartMenuItem* item) {
    uint16_t row_w = width - (SM_ROW_X * 2);
    
    dwm_draw_rect_filled(SM_ROW_X, row_y + 1, row_w, SM_ROW_H - 2, SM_COLOR_ROW);
    dwm_draw_rect(SM_ROW_X, row_y + 1, row_w, SM_ROW_H - 2, SM_COLOR_ROW_BORDER);
    
    uint16_t bullet_x = SM_ROW_X + 10;
    uint16_t bullet_y = row_y + ((SM_ROW_H - SM_BULLET_SIZE) / 2);
    dwm_draw_rect_filled(bullet_x, bullet_y, SM_BULLET_SIZE, SM_BULLET_SIZE, item->bullet_color);
    
    dwm_draw_text(SM_TEXT_X, row_y + ((SM_ROW_H - 8) / 2), item->text, SM_COLOR_TEXT);
}

static void start_menu_draw(struct WindowObject* window) {
    uint16_t w = window->w;
    uint16_t h = window->h;

    // Background + left accent strip
    dwm_draw_rect_filled(0, 0, w, h, SM_COLOR_BG);
    dwm_draw_rect_filled(0, 0, SM_ACCENT_W, h, SM_COLOR_ACCENT);

    // Header
    dwm_draw_rect_filled_gradient_vertical(SM_ACCENT_W, 0, w - SM_ACCENT_W, SM_HEADER_H,
                                           theme.start_menu.header_low, theme.start_menu.header_high);
    dwm_draw_text(SM_TEXT_X - 14, 12, "Start", SM_COLOR_TEXT);
    dwm_draw_text(SM_TEXT_X - 14, 26, "Applications", SM_COLOR_SUBTEXT);
    dwm_draw_line(SM_ACCENT_W, SM_HEADER_H, w - SM_ACCENT_W, 0, SM_COLOR_ACCENT);

    // Main items
    for (uint16_t i = 0; i < SM_MAIN_COUNT; i++) {
        draw_row(w, SM_ITEM_TOP + (i * SM_ROW_H), &main_items[i]);
    }

    // Separator + footer item
    uint16_t sep_y = footer_row_y() - 6;
    dwm_draw_line(SM_ROW_X + 4, sep_y, w - (SM_ROW_X * 2) - 8, 0, SM_COLOR_DIVIDER);
    draw_row(w, footer_row_y(), &footer_item);
}

// Returns the item under a window-local point, or NULL
static const StartMenuItem* start_menu_hit_test(struct WindowObject* window, uint16_t x, uint16_t y) {
    if (x < SM_ROW_X || x >= window->w - SM_ROW_X) return NULL;

    if (y >= SM_ITEM_TOP && y < SM_ITEM_TOP + (SM_MAIN_COUNT * SM_ROW_H)) {
        return &main_items[(y - SM_ITEM_TOP) / SM_ROW_H];
    }

    uint16_t fy = footer_row_y();
    if (y >= fy && y < fy + SM_ROW_H) {
        return &footer_item;
    }

    return NULL;
}

// ==========================================
// Event handler
// ==========================================
void callback_start_menu_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    switch (event) {
        case DWM_EVENT_REDRAW: {
            struct WindowObject* window = dwm_get_window_by_id(handle);
            if (window != NULL) start_menu_draw(window);
            break;
        }

        case DWM_EVENT_MOUSE: {
            if (!(lparam & DWM_STATE_MOUSE_BTN_LEFT)) break;

            struct WindowObject* window = dwm_get_window_by_id(handle);
            if (window == NULL) break;

            uint16_t click_x = (uint16_t)(wparam & 0xFFFF);
            uint16_t click_y = (uint16_t)((wparam >> 16) & 0xFFFF);

            const StartMenuItem* item = start_menu_hit_test(window, click_x, click_y);
            if (item != NULL) {
                dwm_start_menu_close();
                if (item->action != NULL) item->action();
            }
            break;
        }

        case DWM_EVENT_KEYBOARD:
            if ((wparam & 0xFF) == 0x1B) dwm_start_menu_close();
            break;

        // Another window (or the desktop) was clicked
        case DWM_EVENT_FOCUS_LOST:
            dwm_start_menu_close();
            break;

        case DWM_EVENT_DESTROY:
            if (start_menu_handle == handle) start_menu_handle = 0;
            // Pop the taskbar button back up
            dwm_window_send_event(taskbar.window, DWM_EVENT_REDRAW);
            break;

        default:
            break;
    }
}
