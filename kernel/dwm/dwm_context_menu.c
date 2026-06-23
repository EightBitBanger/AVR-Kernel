#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>
#include <kernel/events.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/timer.h>
#include <kernel/util/string.h>

#include <kernel/dwm/dwm_context_menu.h>

// Trigger events

void dwm_icon_right_click(struct WindowContext* ctx) {
    
    uint16_t number_of_items = 4;
    const char* file_menu_options[] = { "Open", "Copy", "Delete", "Properties" };
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_ICON, file_menu_options, number_of_items);
}

void dwm_desktop_right_click(struct WindowContext* ctx) {
    uint16_t number_of_items = 5;
    const char* desktop_menu_options[] = { "Refresh", "New folder" , "New file", "Paste", "Properties" };
    
    dwm_create_context_menu(ctx->mouse.x, ctx->mouse.y, DWM_CONTEXT_MENU_DESKTOP, desktop_menu_options, number_of_items);
    
}

// Event callbacks

void dwm_desktop_context_callback(struct WindowContext* ctx, uint16_t index) {
    switch (index) {
        
    case 0: // Refresh
        
        dwm_summon_message_box("Message", "Refresh");
        break;
        
    case 1: // New folder
        
        dwm_summon_message_box("Message", "new folder");
        break;
        
    case 2: // New file
        
        dwm_summon_message_box("Message", "new file");
        break;
        
    case 3: // Paste
        
        dwm_summon_message_box("Message", "paste");
        break;
        
    case 4: // Display properties / TODO display settings
        
        dwm_summon_message_box("Message", "properties");
        break;
    }
    
}

void dwm_icon_context_callback(struct WindowContext* ctx, uint16_t index) {
    switch (index) {
        
    case 0: // Open
        
        dwm_summon_message_box("icon hit", "open");
        break;
        
    case 1: // Copy
        
        dwm_summon_message_box("icon hit", "copy");
        break;
        
    case 2: // Delete
        
        dwm_destroy_icon(context.focused_icon);
        break;
        
    case 3: // Properties
        
        if (context.focused_icon != NULL) 
            dwm_summon_properties("Properties", context.focused_icon->name, context.focused_icon->path, context.focused_icon->icon_index);
        break;
    }
    
}
