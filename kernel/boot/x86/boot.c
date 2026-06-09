#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/bus/pci.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/boot/x86/gdt.h>

#include <kernel/arch/x86/virtual/vmm.h>

#include <kernel/arch/x86/drivers/ata.h>
#include <kernel/arch/x86/drivers/multiboot_info.h>

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
#include <kernel/panic/panic_error.h>

#define BOOT_DELAY_MS  1000


extern char _kernel_memory_end[];

#define PS2_STATUS_REG         0x64
#define PS2_DATA_REG           0x60
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_MOUSE_DATA  0x20

bool ps2_check_keyboard(void);
void flush_keyboard_buffer(void);
void ps2_route_console(void);


void kmain(uint32_t magic, struct MultibootInfo* mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) 
        return;
    
    if ((mbi->flags & (1 << 11)) == 0) 
        return;
    
    uint32_t framebuffer_pixels      = mbi->framebuffer_width * mbi->framebuffer_height;
    uint32_t framebuffer_size_bytes  = framebuffer_pixels * sizeof(uint32_t);
    
    uint32_t heap_start         = (uint32_t)_kernel_memory_end;
    uint32_t heap_size          = 1024U * 1024U * 32U;
    uint32_t block_size         = 16U;                  // 16 byte aligned
    
    uint32_t front_buffer       = (heap_start + heap_size + 0xFFFU) & ~0xFFFU;
    uint32_t back_buffer        = (front_buffer + framebuffer_size_bytes + 0xFFFU) & ~0xFFFU;
    
    uint32_t identity_map_sz    = back_buffer + 1024U * 1024U * 128U; // + framebuffer_size_bytes
    
    gdt_initiate();
    idt_initiate();
    
    // Initiate SIMD processing
    //init_sse();
    
    // Set millisecond timer
    timer_init();
    
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
    
    // Paging
    pmm_init(mbi);
    vmm_init(mbi, identity_map_sz);
    
    // Map the front frame buffer
    if (mbi->flags & (1 << 12)) { 
        uint32_t vram_bytes = mbi->framebuffer_pitch * mbi->framebuffer_height;
        vmm_map_hardware_region(mbi->framebuffer_addr, front_buffer, vram_bytes, VM_WRITE_COMBINING);
    }
    
    // Initialize the kernels personal heap block
    heap_set_base_address(heap_start);
    heap_init(block_size, heap_size);
    
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
    // Command console boot options
    //
    
    {
        bool activate_console = false;
        draw_flush_display();
        
        uint64_t old_ms = timer_get_ms();
        while ((timer_get_ms() - old_ms) <= BOOT_DELAY_MS) {
            // If there's keyboard data ready, check for 'c'
            if (ps2_check_keyboard()) {
                
                if (kb_getc() == 'c') {
                    activate_console = true;
                    
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
    
    // Blank the screen in preparation of the graphical environment
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), 0xFF000000);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    pci_init();
    
    //
    // Initiate the DWM
    
    dwm_initiate();
    
    
    //
    // DWM Testing
    
    create_folder(30, 30, "system");
    
    // Event system
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // Scalable vector font or MSDF
    
    while(1) {
        dwm_update();
        kernel_event_update();
        
        //__asm__ volatile ("hlt");
        
        
        // TODO move to interrupt handlers later on
        
        if (ps2_check_keyboard()) {
            
            kb_getc();
        }
    }
}



bool ps2_check_keyboard(void) {
    uint8_t status = inb(PS2_STATUS_REG);
    if (status & PS2_STATUS_OUTPUT_FULL) {
        if (status & PS2_STATUS_MOUSE_DATA) {
            mouse_event_handler();
        } else {
            return true; // Keyboard data is ready
        }
    }
    return false;
}

void flush_keyboard_buffer(void) {
    while (inb(PS2_STATUS_REG) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_REG);
    }
}

void ps2_route_console(void) {
    uint8_t status = inb(PS2_STATUS_REG);
    if (status & PS2_STATUS_OUTPUT_FULL) {
        if (status & PS2_STATUS_MOUSE_DATA) {
            mouse_event_handler();
        } else {
            kb_event_handler();
            draw_flush_display();
        }
    }
}
