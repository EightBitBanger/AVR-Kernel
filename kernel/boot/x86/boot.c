#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/paging.h>

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
void callback_handler(WindowHandle handle, wEvent event) {
    counterA++;
    
    switch (event) {
    case EVENT_REDRAW:
        
        dwm_draw_rect(  0, 100, 1024,   32, 0xFFFF0000, true);
        dwm_draw_rect(100,   0,   32, 1024, 0xFF00FF00, true);
        dwm_draw_rect(100, 100, 1024,   32, 0xFF0000FF, true);
        
        char number[16];
        itos(counterA, number);
        
        dwm_draw_rect(10, 10, 100,  40, 0xFF000000, true);
        dwm_draw_text(10, 10, number, 0xFF0AEA0A);
        
        break;
    }
}

void callback_button_handler(WindowHandle handle, wEvent event) {
    
    switch (event) {
    case EVENT_REDRAW:
        
        dwm_draw_rect(  0, 0, 1024, 1024, 0xFF222222, true);
        
        dwm_draw_text(10, 10, "ok", 0xFF0AEA0A);
        
        break;
    }
}

void desktop_environment_init(void) {
    
    struct WindowClass wclass;
    wclass.x = 200;
    wclass.y = 100;
    wclass.width = 250;
    wclass.height = 250;
    wclass.event_handler = callback_handler;
    
    WindowHandle window = create_window(wclass, 0);
    
    
    
    struct WindowClass wclassbutton;
    wclassbutton.x = 100;
    wclassbutton.y = 300;
    wclassbutton.width = 80;
    wclassbutton.height = 30;
    wclassbutton.event_handler = callback_button_handler;
    
    WindowHandle button = create_window(wclassbutton, WINDOW_STYLE_NOBORDERS | WINDOW_STYLE_TOPMOST);
    
    
    //dwm_window_set_parent(button, window);
    
    
}


void init_sse(void) {
    uint32_t cr0;
    uint32_t cr4;
    
    // 1. Read current CR0
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    
    // Clear the EM (Emulation) bit (bit 2) -> We have a real FPU, don't emulate it
    cr0 &= ~(1 << 2);
    // Set the MP (Monitor Coprocessor) bit (bit 1) -> Controls interaction with TS bit
    cr0 |= (1 << 1);
    
    // Write back to CR0
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    
    // 2. Read current CR4
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    
    // Set OSFXSR (bit 9) -> Tells the OS that it supports FXSAVE/FXRSTOR for SSE state
    cr4 |= (1 << 9);
    // Set OSXMMEXCPT (bit 10) -> Tells the CPU to use unmasked SIMD floating-point exceptions (#XM)
    cr4 |= (1 << 10);
    
    // Write back to CR4
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
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
    
    //init_sse();
    
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
    
    paging_initiate(mbi_info);
    
    // Calculate dimensions exactly like you used to
    uint32_t fb_pixels = mbi_info->framebuffer_width * mbi_info->framebuffer_height;
    uint32_t fb_size_bytes = fb_pixels * sizeof(uint32_t);
    
    // Lock the Graphics back buffer at the 8MB mark
    // This is well within our 64MB chunk
    uint32_t* forced_back_buffer = (uint32_t*)0x00800000;
    
    extern void draw_set_frame_buffer(uint32_t* buffer_ptr);
    draw_set_frame_buffer(forced_back_buffer);
    
    uint32_t forced_heap_start = 0x01800000; 
    heap_set_base_address(forced_heap_start);
    
    // Initialize an 8 MB heap block (spans from 24MB to 32MB)
    uint32_t heap_sz  = (1024 * 1024 * 8); 
    uint32_t block_sz = 16;
    heap_init(block_sz, heap_sz);
    
    draw_set_buffer_default();
    
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
    
    desktop_environment_init();
    
    create_folder(30, 30, "new folder");
    
    
    // Event system
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // Scalable vector font
    
    // Buttons
    
    while(1) {
        
        dwm_update();
        
        __asm__ volatile ("hlt");
        
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

