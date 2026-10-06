#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/events.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#include <kernel/dwm/dwm_context_menu.h>

/**
 * Generates a unique file/folder name and full path by appending index numbers 
 * (e.g., "new_folder (1)") if an item with the same name already exists.
 */
void name_get_unique(const char* dir_path, const char* base_name, char* out_name, char* out_path) {
    int count = 0;

    // Detect file extension (ignore leading dot for hidden files)
    const char* dot = strrchr(base_name, '.');
    if (dot == base_name) {
        dot = NULL;
    }

    while (1) {
        if (count == 0) {
            strncpy(out_name, base_name, DWM_MAX_PATH_LEN - 1);
            out_name[DWM_MAX_PATH_LEN - 1] = '\0';
        } else if (dot != NULL) {
            int name_len = (int)(dot - base_name);
            snprintf(out_name, DWM_MAX_PATH_LEN, "%.*s (%d)%s", name_len, base_name, count, dot);
        } else {
            snprintf(out_name, DWM_MAX_PATH_LEN, "%s (%d)", base_name, count);
        }

        // Construct full target path
        memset(out_path, 0, DWM_MAX_PATH_LEN);
        strncpy(out_path, dir_path, DWM_MAX_PATH_LEN - 1);

        size_t dir_len = strlen(out_path);
        if (dir_len > 0 && out_path[dir_len - 1] != '/') {
            strncat(out_path, "/", DWM_MAX_PATH_LEN - strlen(out_path) - 1);
        }
        strncat(out_path, out_name, DWM_MAX_PATH_LEN - strlen(out_path) - 1);

        // Check if path is available
        if (!vfs_exists(out_path)) {
            break;
        }

        count++;
    }
}

bool dwm_desktop_create_folder(void) {
    char dir_path[DWM_MAX_PATH_LEN];
    memset(dir_path, '\0', sizeof(dir_path));

    struct LocalPaths paths;
    kernel_get_local_paths(&paths);

    strncat(dir_path, paths.home, DWM_MAX_PATH_LEN - 1);
    strncat(dir_path, "/usr/desktop/", DWM_MAX_PATH_LEN - strlen(dir_path) - 1);

    char item_name[DWM_MAX_PATH_LEN];
    char item_path[DWM_MAX_PATH_LEN];

    // Generate unique name and full path
    name_get_unique(dir_path, "new_folder", item_name, item_path);

    if (!vfs_mkdir(item_path)) 
        return false;

    dwm_create_folder(input.mouse_last.x, input.mouse_last.y, item_name, item_path);

    kernel_event_send(KEVENT_DWM_REFRESH, "", "");
    return true;
}

bool dwm_desktop_create_file(void) {
    char dir_path[DWM_MAX_PATH_LEN];
    memset(dir_path, '\0', sizeof(dir_path));

    struct LocalPaths paths;
    kernel_get_local_paths(&paths);

    strncat(dir_path, paths.home, DWM_MAX_PATH_LEN - 1);
    strncat(dir_path, "/usr/desktop/", DWM_MAX_PATH_LEN - strlen(dir_path) - 1);

    char item_name[DWM_MAX_PATH_LEN];
    char item_path[DWM_MAX_PATH_LEN];

    // Generate unique name and full path
    name_get_unique(dir_path, "new_file", item_name, item_path);

    File item_file = vfs_open(item_path, VFS_OPEN_CREATE);
    if (item_file == VFS_INVALID_FILE) 
        return false;
    vfs_close(item_file);

    dwm_create_file(input.mouse_last.x, input.mouse_last.y, item_name, item_path);

    kernel_event_send(KEVENT_DWM_REFRESH, "", "");
    return true;
}

// Desktop Actions

static void action_desktop_refresh(struct WindowContext* ctx) { 
    kernel_event_send(KEVENT_DWM_REFRESH, "", "");
}

static void action_desktop_new_folder(struct WindowContext* ctx) { 
    dwm_desktop_create_folder();
}

static void action_desktop_new_file(struct WindowContext* ctx) { 
    dwm_desktop_create_file();
}

static void action_desktop_paste(struct WindowContext* ctx) {
    struct LocalPaths paths;
    kernel_get_local_paths(&paths);
    if (paths.home[0] == '\0') {
        dwm_summon_message_box("Paste", "There is no desktop folder to paste into");
        return;
    }
    
    char dir_path[DWM_MAX_PATH_LEN];
    memset(dir_path, '\0', sizeof(dir_path));
    strncpy(dir_path, paths.home, DWM_MAX_PATH_LEN - 1);
    strncat(dir_path, "/usr/desktop", DWM_MAX_PATH_LEN - strlen(dir_path) - 1);
    
    // First pasted icon goes where the menu was opened, the rest on free spots
    struct ContextMenu* menu = &ctxmenu.menus[0];
    dwm_clipboard_paste_into(dir_path, menu->x, menu->y);
}

static void action_desktop_properties(struct WindowContext* ctx) { 
    dwm_summon_message_box("Message", "properties"); 
}

// Icon Actions
static void action_icon_open(struct WindowContext* ctx)  {
    if (context.focused_icon != NULL) {
        if (vfs_directory_check(context.focused_icon->path)) {
            kernel_event_send(KEVENT_EXECUTE, "explorer", context.focused_icon->path);
        } else {
            kernel_event_send(KEVENT_EXECUTE, "notepad", context.focused_icon->path);
        }
    }
}

static void action_icon_cut(struct WindowContext* ctx) {
    if (context.focused_icon != NULL) {
        dwm_clipboard_put_file(context.focused_icon->path, DWM_CLIPBOARD_OP_CUT);
    }
}

static void action_icon_copy(struct WindowContext* ctx) {
    if (context.focused_icon != NULL) {
        dwm_clipboard_put_file(context.focused_icon->path, DWM_CLIPBOARD_OP_COPY);
    }
}

static void action_icon_rename(struct WindowContext* ctx) {
    if (context.focused_icon != NULL) {
        dwm_desktop_rename_begin(context.focused_icon);
    }
}

static void action_icon_delete(struct WindowContext* ctx) {
    if (context.focused_icon != NULL) {
        dwm_summon_dialog_delete(
            "Deletion request", 
            context.focused_icon->path, 
            0, // Pass 0 (no parent) instead of context.event_window->id to prevent NULL dereference
            context.focused_icon->icon_index
        );
    }
}

static void action_icon_properties(struct WindowContext* ctx) {
    if (context.focused_icon != NULL) {
        dwm_summon_properties("Properties", context.focused_icon->name, context.focused_icon->path, context.focused_icon->icon_index);
    }
}

// Define menu schemas

static const ContextMenuItem desktop_menu[] = {
    { "Refresh",    action_desktop_refresh },
    { "New folder", action_desktop_new_folder },
    { "New file",   action_desktop_new_file },
    { "Paste",      action_desktop_paste },
    { "Properties", action_desktop_properties }
};

static const ContextMenuItem icon_menu[] = {
    { "Open",       action_icon_open },
    { "Cut",        action_icon_cut },
    { "Copy",       action_icon_copy },
    { "Rename",     action_icon_rename },
    { "Delete",     action_icon_delete },
    { "Properties", action_icon_properties }
};

#define DESKTOP_MENU_SIZE (sizeof(desktop_menu) / sizeof(desktop_menu[0]))
#define ICON_MENU_SIZE (sizeof(icon_menu) / sizeof(icon_menu[0]))

// Trigger events (right click handlers)

void dwm_icon_right_click(struct WindowContext* ctx) {
    const char* labels[ICON_MENU_SIZE];
    for (uint16_t i = 0; i < ICON_MENU_SIZE; i++) {
        labels[i] = icon_menu[i].text;
    }
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_ICON, labels, ICON_MENU_SIZE);
}

void dwm_desktop_right_click(struct WindowContext* ctx) {
    const char* labels[DESKTOP_MENU_SIZE];
    for (uint16_t i = 0; i < DESKTOP_MENU_SIZE; i++) {
        labels[i] = desktop_menu[i].text;
    }
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_DESKTOP, labels, DESKTOP_MENU_SIZE);
}

// Event callbacks

void dwm_desktop_context_callback(struct WindowContext* ctx, uint16_t index) {
    if (index < DESKTOP_MENU_SIZE && desktop_menu[index].action != NULL) {
        desktop_menu[index].action(ctx);
    }
}

void dwm_icon_context_callback(struct WindowContext* ctx, uint16_t index) {
    if (index < ICON_MENU_SIZE && icon_menu[index].action != NULL) {
        icon_menu[index].action(ctx);
    }
}
