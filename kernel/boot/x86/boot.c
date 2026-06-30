#include <stddef.h>
#include <stdint.h>

#include <kernel/kernel.h>

// Platform
#include <kernel/arch/x86/io.h>
#include <kernel/boot/x86/gdt.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/boot/x86/multiboot_info.h>

// Memory
#include <kernel/memory/malloc.h>
#include <kernel/arch/x86/virtual/vmm.h>

// Buses
#include <kernel/arch/x86/bus/pci.h>

// Drivers
#include <kernel/arch/x86/drivers/ata/ata.h>
#include <kernel/arch/x86/drivers/ps2.h>

// Utility
#include <kernel/util/math.h>
#include <kernel/util/string.h>
#include <kernel/util/timer.h>

// Console
#include <kernel/console/print.h>
#include <kernel/console/console.h>
#include <kernel/console/virtual_key.h>

#include <kernel/dwm/dwm.h>
#include <kernel/panic/panic_error.h>

#define BOOT_DELAY_MS  500

// Position in memory where the kernel ends
extern char _kernel_program_end[];

void kmain(uint32_t magic, struct MultibootInfo* mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) 
        return;
    
    if ((mbi->flags & (1 << 11)) == 0) 
        return;
    
    uint32_t framebuffer_pixels      = mbi->framebuffer_width * mbi->framebuffer_height;
    uint32_t framebuffer_size_bytes  = framebuffer_pixels * sizeof(uint32_t);
    
    // 16 byte aligned
    uint32_t heap_start              = ((uint32_t)_kernel_program_end + 0xFU) & ~0xFU;
    uint32_t heap_size               = 1024U * 1024U * 4U;
    uint32_t block_size              = 16U;
    
    // 4k page aligned
    uint32_t front_buffer            = (heap_start + heap_size + 0xFFFU) & ~0xFFFU;
    uint32_t back_buffer             = (front_buffer + framebuffer_size_bytes + 0xFFFU) & ~0xFFFU;
    
    uint32_t _kernel_memory_end      = (back_buffer + framebuffer_size_bytes + 0xFFFU) & ~0xFFFU;
    
    gdt_init();
    idt_init();
    
    // Set millisecond timer
    timer_init();
    
    // Paging
    pmm_init(mbi, _kernel_memory_end);
    vmm_init(mbi, _kernel_memory_end);
    
    // Initialize the kernels personal heap block
    heap_set_base_address(heap_start);
    heap_init(block_size, heap_size);
    
    // Initiate display and drawing
    draw_set_info((uint32_t)mbi);
    display_init();
    
    display_cursor_set_line(0);
    display_cursor_set_position(0);
    
    // Set drawing frame buffers
    draw_set_clip_rect(0, 0, mbi->framebuffer_width, mbi->framebuffer_height);
    draw_set_frame_front_buffer(front_buffer);
    draw_set_frame_back_buffer(back_buffer);
    draw_set_buffer_default();
    
    // Map the graphics front frame buffer 
    // as a cached write combine buffer
    if (mbi->flags & (1 << 12)) { 
        uint32_t vram_bytes = mbi->framebuffer_pitch * mbi->framebuffer_height;
        vmm_map_hardware_region(mbi->framebuffer_addr, front_buffer, vram_bytes, VM_WRITE_COMBINING);
    }
    
    // Initiate keyboard and mouse
    static char keyboard_string[255];
    static char prompt_string[255];
    static char virtual_key_map[255];
    
    kb_init();
    kb_map_init(virtual_key_map, sizeof(virtual_key_map));
    
    mouse_initiate();
    mouse_set_cursor_speed(14, 14);
    mouse_set_cursor_acceleration(2);
    
    // Prepare the console and fire up the kernel
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    kernel_init();
    
    print("kernel v0.0.0\n");
    draw_flush_display();
    
    
    //
    // Command console / boot options
    
    {
        bool activate_console = false;
        
        uint64_t old_ms = timer_get_ms();
        while ((timer_get_ms() - old_ms) <= BOOT_DELAY_MS) {
            // Check keyboard data ready
            if (ps2_check_keyboard()) {
                
                if (kb_getc() == 'c') {
                    activate_console = true;
                    
                    // Scan PCI bus for available hardware
                    // before entering the console
                    pci_init();
                    
                    flush_keyboard_buffer();
                    
                    console_prompt_print();
                    draw_flush_display();
                    break;
                }
                draw_flush_display();
            }
        }
        
        // Run the dedicated console mode if activated
        while (activate_console) {
            ps2_route_console();
        }
    }
    
    // Scan PCI bus for available hardware
    pci_init();
    
    // Courtesy delay for boot output
    uint64_t old_ms = timer_get_ms();
    while ((timer_get_ms() - old_ms) <= BOOT_DELAY_MS);
    
    // Blank the screen in preparation for pure graphics mode
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), 0xFF000000);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    //
    // Initiate the DWM graphical environment
    
    dwm_initiate();
    
    uint16_t sep = 90;
    uint16_t posx = 30;
    uint16_t posy = 30;
    
    dwm_create_folder(posx, posy, "mount",      "/mnt"); posx += sep;
    //dwm_create_folder(posx, posy, "devices",    "/dev/pci"); posx += sep;
    //dwm_create_folder(posx, posy, "processes",  "/proc"); posx += sep;
    
    //dwm_create_mount(posx, posy,  "ssd0",       "/mnt/ssd0");
    
    
    
    // User event call back messaging
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // TODO scalable vector font or MSDF
    
    // TODO color themes to handle color
    
    // TODO key combination binding
    
    while(1) {
        
        dwm_update();
        
        kernel_event_update();
        
        // TODO move to interrupt handlers later on
        if (ps2_check_keyboard()) {
            
            uint16_t last_key_pressed = kb_getc();
            
            dwm_set_keyboard_char(last_key_pressed);
            
            dwm_event_send(DWM_EVENT_KEYBOARD);
        }
        
    }
}
