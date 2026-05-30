#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/boot/x86/gdt.h>

#include <kernel/kernel.h>
#include <kernel/util/math.h>
#include <kernel/util/string.h>
#include <kernel/util/timer.h>

#include <kernel/console/print.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/mouse.h>
#include <kernel/console/console.h>
#include <kernel/console/virtual_key.h>

#include <kernel/dwm/dwm.h>

#define BOOT_DELAY_MS  1000


extern char _kernel_memory_end[];

extern uint32_t display_width;
extern uint32_t display_height;

uint32_t counterA=0;
void callback_handlerA(WindowHandle handle, wEvent event) {
    counterA++;
    
    switch (event) {
    case EVENT_REDRAW:
        
        dwm_draw_rect(  0, 100, 200,  32, 0xFFFF0000, true);
        dwm_draw_rect(100,   0,  32, 200, 0xFF00FF00, true);
        dwm_draw_rect(100, 100, 200,  32, 0xFF0000FF, true);
        
        char number[16];
        itos(counterA, number);
        
        dwm_draw_rect(10, 10, 100,  40, 0xFF000000, true);
        dwm_draw_text(10, 10, number, 0xFF0AEA0A);
        
        break;
    }
}

uint32_t counterB=0;
void callback_handlerB(WindowHandle handle, wEvent event) {
    counterB++;
    
    switch (event) {
    case EVENT_REDRAW:
        
        dwm_draw_rect(  0, 100, 200,  32, 0xFFFF0000, true);
        dwm_draw_rect(100,   0,  32, 200, 0xFF00FF00, true);
        dwm_draw_rect(100, 100, 200,  32, 0xFF0000FF, true);
        
        char number[16];
        itos(counterB, number);
        
        dwm_draw_rect(10, 10, 100,  40, 0xFF000000, true);
        dwm_draw_text(10, 10, number, 0xFF0AEA0A);
        
        break;
    }
}


void kmain(uint32_t magic, struct MultibootInfo* mbi_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) 
        return;
    
    // Verify GRUB successfully gave us a Linear Framebuffer (Bit 11 check)
    if ((mbi_info->flags & (1 << 11)) == 0) 
        return;
    
    // Set the table for the kernel
    gdt_initiate();
    idt_initiate();
    
    // Millisecond timer
    timer_init();
    
    char keyboard_string[255];
    char prompt_string[255];
    char virtual_key_map[255];
    
    //
    // Frame buffer
    
    draw_set_info((uint32_t)mbi_info);
    
    display_init();
    draw_set_clip_rect(0, 0, display_width, display_height);
    
    // Calculate the raw size of your framebuffer in bytes
    uint32_t fb_pixels = (uint32_t)display_get_width() * (uint32_t)display_get_height();
    uint32_t fb_size_bytes = fb_pixels * sizeof(uint32_t);
    
    uint32_t frame_buffer_back_start = ((uint32_t)_kernel_memory_end + 4095) & ~4095;
    uint32_t* frame_buffer_back = (uint32_t*)frame_buffer_back_start;
    
    draw_set_frame_buffer(frame_buffer_back);
    draw_set_buffer_default();
    
    
    
    //
    // Heap allocator
    
    // Move the heap start to after the framebuffer, and align it again
    uint32_t heap_start = (frame_buffer_back_start + fb_size_bytes + 4095) & ~4095;
    
    // Your original heap config
    uint32_t heap_sz  = (((uint32_t)1024 * (uint32_t)1024 * (uint32_t)1024) * 2);
    uint32_t block_sz = 16;
    
    heap_set_base_address(heap_start);
    heap_init(block_sz, heap_sz);
    
    //
    // Display and keyboard for the console
    
    display_clear();
    /*
    
    while(1) {
        
        draw_rect_filled(0, 0, display_width, display_height, 0xFF0000C0);
        draw_flush_region(0, 0, display_width, display_height);
        
        draw_rect_filled(0, 0, display_width, display_height, 0xFF00C000);
        draw_flush_region(0, 0, display_width, display_height);
        
    }
    */
    
    display_cursor_set_line(0);
    display_cursor_set_position(0);
    
    kb_init();
    kb_map_init(virtual_key_map, sizeof(virtual_key_map));
    
    mouse_initiate();
    mouse_set_cursor_speed(12, 12);
    mouse_set_cursor_acceleration(2);
    
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    kernel_init();
    
    console_prompt_set_string("/>");
    print("kernel v0.0.0\n");
    draw_flush_region(0, 0, display_width, display_height);
    
    //
    // Command console boot option
    
    {
    
    bool activate_console = false;
    draw_flush_region(0, 0, display_width, display_height);
    
    uint64_t old_ms = timer_get_ms();
    while(1) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            // If bit 5 is 1 this is a mouse byte
            if (status & 0x20) {
                mouse_event_handler();
            } else {
                
                if (kb_getc() == 'c') {
                    activate_console = true;
                    
                    console_prompt_print();
                    draw_flush_region(0, 0, display_width, display_height);
                    break;
                }
                
                draw_flush_region(0, 0, display_width, display_height);
            }
        }
        
        uint64_t current_ms = timer_get_ms();
        if ((current_ms - old_ms) > BOOT_DELAY_MS) 
            break;
    }
    
    while(activate_console) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            // If bit 5 is 1 this is a mouse byte
            if (status & 0x20) {
                mouse_event_handler();
            } else {
                kb_event_handler();
                draw_flush_region(0, 0, display_width, display_height);
            }
        }
    }
    
    }
    
    //
    // Initiate desktop environment
    
    dwm_initiate();
    
    uint32_t* cursor_sprite = malloc(sizeof(uint32_t) * rc_cursor_pointer.width * rc_cursor_pointer.height);
    sprite_get_bitmap(cursor_sprite, &rc_cursor_pointer);
    
    dwm_set_cursor(cursor_sprite, rc_cursor_pointer.width, rc_cursor_pointer.height);
    
    
    WindowHandle handleA = create_window(200, 100, 640, 480, callback_handlerA);
    WindowHandle handleB = create_window(500, 200, 200, 200, callback_handlerB);
    
    
    uint32_t* file_sprite = malloc(sizeof(uint32_t) * rc_icon_file.width * rc_icon_file.height);
    sprite_get_bitmap(file_sprite, &rc_icon_file);
    
    uint32_t* sprite_file_system = malloc(sizeof(uint32_t) * rc_icon_system.width * rc_icon_system.height);
    sprite_get_bitmap(sprite_file_system, &rc_icon_system);
    
    uint32_t* sprite_file_document = malloc(sizeof(uint32_t) * rc_icon_document.width * rc_icon_document.height);
    sprite_get_bitmap(sprite_file_document, &rc_icon_document);
    
    uint32_t* folder_sprite = malloc(sizeof(uint32_t) * rc_icon_folder.width * rc_icon_folder.height);
    sprite_get_bitmap(folder_sprite, &rc_icon_folder);
    
    struct IconObject* file = create_icon(150, 100, rc_icon_file.width, rc_icon_file.height, file_sprite);
    struct IconObject* system = create_icon(200, 100, rc_icon_system.width, rc_icon_system.height, sprite_file_system);
    struct IconObject* document = create_icon(250, 100, rc_icon_document.width, rc_icon_document.height, sprite_file_document);
    struct IconObject* folder = create_icon(300, 100, rc_icon_folder.width, rc_icon_folder.height, folder_sprite);
    
    
    extern void dwm_calculate_icon_bounds(struct IconObject* icon);
    
    strcpy(folder->name, "new folder");
    dwm_calculate_icon_bounds(folder);
    
    // TODO add an internal icon renaming function
    
    
    
    // Event system
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // Context menus
    
    // Scalable vector font
    
    // Buttons
    
    while(1) {
        
        dwm_update();
        
        
        // Handle mouse and keyboard input
        // TODO move to interrupt handlers later on
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            // If bit 5 is 1 this is a mouse byte
            if (status & 0x20) {
                mouse_event_handler();
            } else {
                kb_getc(); // Burn keyboard input
            }
        }
        
    }
}

