#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/events.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#include <kernel/dwm/dwm_context_menu.h>

// Individual menu actions

// Desktop Actions
static void action_desktop_refresh(struct WindowContext* ctx)     { dwm_summon_message_box("Message", "Refresh"); }
static void action_desktop_new_folder(struct WindowContext* ctx)  { dwm_summon_message_box("Message", "new folder"); }
static void action_desktop_new_file(struct WindowContext* ctx)    { dwm_summon_message_box("Message", "new file"); }
static void action_desktop_paste(struct WindowContext* ctx)       { dwm_summon_message_box("Message", "paste"); }
static void action_desktop_properties(struct WindowContext* ctx)  { dwm_summon_message_box("Message", "properties"); }

// Icon Actions
static void action_icon_open(struct WindowContext* ctx)  { dwm_summon_message_box("icon hit", "open"); }
static void action_icon_copy(struct WindowContext* ctx)  { dwm_summon_message_box("icon hit", "copy"); }
static void action_icon_delete(struct WindowContext* ctx){ dwm_destroy_icon(context.focused_icon); }
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
    { "Copy",       action_icon_copy },
    { "Delete",     action_icon_delete },
    { "Properties", action_icon_properties }
};


#define DESKTOP_MENU_SIZE (sizeof(desktop_menu) / sizeof(desktop_menu[0]))
#define ICON_MENU_SIZE (sizeof(icon_menu) / sizeof(icon_menu[0]))

// Trigger events (right click handlers)

void dwm_icon_right_click(struct WindowContext* ctx) {
    // Extract just the string labels from our structured array dynamically
    const char* labels[ICON_MENU_SIZE];
    for (uint16_t i = 0; i < ICON_MENU_SIZE; i++) {
        labels[i] = icon_menu[i].text;
    }
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_ICON, labels, ICON_MENU_SIZE);
}

void dwm_desktop_right_click(struct WindowContext* ctx) {
    // Extract just the string labels from our structured array dynamically
    const char* labels[DESKTOP_MENU_SIZE];
    for (uint16_t i = 0; i < DESKTOP_MENU_SIZE; i++) {
        labels[i] = desktop_menu[i].text;
    }
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_DESKTOP, labels, DESKTOP_MENU_SIZE);
}


// Event callbacks

void dwm_desktop_context_callback(struct WindowContext* ctx, uint16_t index) {
    // Safety check: ensure the callback index falls within our defined configuration array
    if (index < DESKTOP_MENU_SIZE && desktop_menu[index].action != NULL) {
        desktop_menu[index].action(ctx);
    }
}

void dwm_icon_context_callback(struct WindowContext* ctx, uint16_t index) {
    // Safety check: ensure the callback index falls within our defined configuration array
    if (index < ICON_MENU_SIZE && icon_menu[index].action != NULL) {
        icon_menu[index].action(ctx);
    }
}
