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

extern char _kernel_memory_end[];

extern uint32_t display_width;
extern uint32_t display_height;

void callback_handler(struct WindowHandle* window) {
    
    draw_rect_filled(window->surface_x, window->surface_y, window->surface_w, window->surface_h, 0xFF000A0A);
    draw_rect_filled(window->surface_x + 10, window->surface_y + 10, 32, 32, 0xFF5A3A2F);
    
    draw_circle(window->surface_x + 10, window->surface_y + 10, 10, 0xFFFE3A2E);
    
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
    
    char keyboard_string[255];
    char prompt_string[255];
    char virtual_key_map[255];
    
    //
    // Frame buffer
    
    draw_set_info((uint32_t)mbi_info);
    display_init();
    draw_set_clip_rect(0, 0, display_width, display_height);
    
    // Calculate framebuffer size in bytes
    uint32_t fb_pixels = (uint32_t)display_get_width() * (uint32_t)display_get_height();
    uint32_t fb_size_bytes = fb_pixels * sizeof(uint32_t);
    
    // Align the start of the framebuffer right after the kernel
    uint32_t frame_buffer_back_start = ((uint32_t)_kernel_memory_end + 4095) & ~4095;
    uint32_t* frame_buffer_back = (uint32_t*)frame_buffer_back_start;
    
    //
    // Heap allocator
    
    // Move the heap start to after the framebuffer, and align it again
    uint32_t heap_start = (frame_buffer_back_start + fb_size_bytes + 4095) & ~4095;
    
    // Your original heap config
    uint32_t heap_sz  = ((uint32_t)1024 * (uint32_t)1024 * (uint32_t)10241);
    uint32_t block_sz = 64;
    
    heap_set_base_address(heap_start);
    heap_init(block_sz, heap_sz);
    
    //
    // Display and keyboard for the console
    
    draw_set_frame_buffer(frame_buffer_back);
    
    display_clear();
    
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
    
    bool activate_console = false;
    draw_flush_region(0, 0, display_width, display_height);
    
    for (int i = 0; i < 70; i++) {
        delay_ms(10);
        
        // Check if data is waiting in the PS/2 controller
        if (!(inb(0x64) & 0x01)) 
            continue;
        
        // Ensure it's keyboard data, not mouse data
        if (inb(0x64) & 0x20) 
            continue;
        
        if (kb_getc() == 'c') {
            activate_console = true;
            
            console_prompt_print();
            draw_flush_region(0, 0, display_width, display_height);
            break;
        }
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
    
    dwm_initiate();
    
    print("\n\n");
    
    for (unsigned int i=1 ; i < 4; i++) 
        create_window(75 * i, 75 * i, 420, 340);
    
    struct WindowHandle* handle = create_window(500, 200, 640, 480);
    handle->event_callback = callback_handler;
    
    
    while(1) {
        
        dwm_update();
        
        // Handle mouse and keyboard input
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

