#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <kernel/dwm/dwm.h>
#include <kernel/events.h>


void callback_handler_main(WindowHandle handle, wEvent event, uint16_t wparam) {
    switch (event) {
    case EVENT_RESIZE:
        //dwm_window_send_event(handle, EVENT_CLOSE);
        break;
        
    case EVENT_REDRAW:
        break;
    }
}

void callback_item_handler(WindowHandle handle, wEvent event, uint16_t wparam) {
    switch (event) {
    case EVENT_RESIZE:
        //dwm_window_send_event(dwm_window_get_parent(handle), EVENT_CLOSE);
        break;
    case EVENT_MOUSE:
        
        kernel_event_send(KEVENT_EXECUTE, "event");
        
        break;
        
    case EVENT_REDRAW:
        struct Image* folder_image = dwm_resource_find("icon_folder");
        
        dwm_draw_sprite(0, 0, folder_image);
        
        //dwm_draw_text(50, 80, "new folder", 0xFFFFFFFF);
        break;
    }
}


void explorer_main(void) {
    
    WindowClass wclass;
    memset(&wclass, 0x00, sizeof(WindowClass));
    
    wclass.x = 200;
    wclass.y = 150;
    wclass.width = 400;
    wclass.height = 280;
    
    wclass.max_width = 400;
    wclass.max_height = 370;
    
    wclass.title = "Explorer";
    
    WindowHandle window = create_window(wclass, 0, callback_handler_main);
    
    {
    WindowClass wclassbutton;
    wclassbutton.x = 1;
    wclassbutton.y = 1;
    wclassbutton.width = 100;
    wclassbutton.height = 100;
    WindowHandle button = create_window(wclassbutton, WINDOW_STYLE_NOBORDERS | WINDOW_STYLE_NOCLOSEBOX | WINDOW_STYLE_CHILD, callback_item_handler);
    dwm_window_set_parent(button, window);
    }
    
    {
    WindowClass wclassbutton;
    wclassbutton.x = 200;
    wclassbutton.y = 1;
    wclassbutton.width = 100;
    wclassbutton.height = 100;
    WindowHandle button = create_window(wclassbutton, WINDOW_STYLE_NOBORDERS | WINDOW_STYLE_NOCLOSEBOX | WINDOW_STYLE_CHILD, callback_item_handler);
    dwm_window_set_parent(button, window);
    }
    
    /*
    unsigned int number_of_items = 8;
    // Define the grid layout constraints
    unsigned int max_columns = 4;      // Fits nicely inside a 400px wide window
    uint16_t item_width = 70;          // Slightly smaller than cell width to leave a gap
    uint16_t item_height = 70;         // Slightly smaller than cell height to leave a gap
    uint16_t cell_width = 90;          // Horizontal spacing between items
    uint16_t cell_height = 85;         // Vertical spacing between items
    uint16_t margin_x = 15;            // Left padding from window border
    uint16_t margin_y = 15;            // Top padding from window border
    
    for (unsigned int i = 0; i < number_of_items; i++) {
        // Calculate current column and row based on index 'i'
        uint16_t column = i % max_columns;
        uint16_t row = i / max_columns;
        
        WindowClass wclassbutton;
        wclassbutton.x = margin_x + (column * cell_width);
        wclassbutton.y = margin_y + (row * cell_height);
        wclassbutton.width = item_width;
        wclassbutton.height = item_height;
        
        WindowHandle button = create_window(wclassbutton, WINDOW_STYLE_NOBORDERS | WINDOW_STYLE_NOCLOSEBOX | WINDOW_STYLE_CHILD, callback_item_handler);
        dwm_window_set_parent(button, window);
    }
    */
}
