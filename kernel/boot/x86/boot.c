#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/paging.h>
#include <kernel/arch/x86/page_alloc.h>
#include <kernel/arch/x86/bus/pci.h>
#include <kernel/boot/x86/interrupt.h>
#include <kernel/boot/x86/gdt.h>

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

extern uint32_t display_width;
extern uint32_t display_height;






void callback_handler(WindowHandle handle, wEvent event) {
    switch (event) {
    case EVENT_REDRAW:
        dwm_draw_rect_filled(0, 0, 1024, 1024, 0xFF080808);
        
        dwm_draw_text(5, 10, "Fancy message box...", 0xFFFFFFFF);
        break;
    }
}

void callback_button_handler(WindowHandle handle, wEvent event) {
    switch (event) {
    case EVENT_MOUSE:
        dwm_window_send_event(dwm_window_get_parent(handle), EVENT_CLOSE);
        break;
        
    case EVENT_REDRAW:
        dwm_draw_rect_filled(0, 0, 65, 27, 0xFF555555);
        dwm_draw_rect_filled(1, 1, 63, 25, 0xFF222282);
        
        dwm_draw_text(25, 9, "ok", 0xFF0AEA0A);
        
        break;
    }
}

void desktop_environment_init(void) {
    
    WindowClass wclass;
    wclass.x = 200 + 30;
    wclass.y = 100 + 40;
    wclass.width = 200;
    wclass.height = 100;
    WindowHandle window = create_window(wclass, 0, callback_handler);
    
    WindowClass wclassbutton;
    wclassbutton.x = 67;
    wclassbutton.y = 56;
    wclassbutton.width = 65;
    wclassbutton.height = 27;
    WindowHandle button = create_window(wclassbutton, WINDOW_STYLE_NOBORDERS | WINDOW_STYLE_NOCLOSEBOX, callback_button_handler);
    
    dwm_window_set_parent(button, window);
    
}


void init_sse(void) {
    uint32_t cr0;
    uint32_t cr4;
    
    // Read current CR0
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    
    // Clear the EM (Emulation) bit (bit 2) -> We have a real FPU, don't emulate it
    cr0 &= ~(1 << 2);
    // Set the MP (Monitor Coprocessor) bit (bit 1) -> Controls interaction with TS bit
    cr0 |= (1 << 1);
    
    // Write back to CR0
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    
    // Read current CR4
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    
    // Set OSFXSR (bit 9) -> FXSAVE/FXRSTOR support for SSE state
    cr4 |= (1 << 9);
    // Set OSXMMEXCPT (bit 10) -> Use unmasked SIMD floating-point exceptions (#XM)
    cr4 |= (1 << 10);
    
    // Write back to CR4
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}



void debug_dump_storage_first_bytes(void) {
    uint16_t io_base = 0x1F0; // Legacy primary IDE base port
    uint16_t buffer[256];     // 256 words = 512 bytes
    
    // Select Master Drive and use LBA addressing mode
    outb(io_base + 6, 0xE0);
    
    // Set sector count to 1
    outb(io_base + 2, 1);
    
    // Set LBA address to 0 (Sector 0: the very beginning of the raw chunk)
    outb(io_base + 3, 0x00); // LBA Low
    outb(io_base + 4, 0x00); // LBA Mid
    outb(io_base + 5, 0x00); // LBA High
    
    // Send the "Read Sectors with Retry" Command (0x20)
    outb(io_base + 7, 0x20);
    
    // Poll Status Register: Wait for BUSY (0x80) to clear
    while (inb(io_base + 7) & 0x80);
    
    // Poll Status Register: Wait for DRQ (Data Request, 0x08) to set
    while (!(inb(io_base + 7) & 0x08));
    
    // Read 256 words from the Data Port into our buffer
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(io_base + 0);
    }
    
    // Print the bytes out in a clean hex formatting grid
    uint8_t* byte_ptr = (uint8_t*)buffer;
    print("\n\n");
    
    for (int i = 0; i < 64; i++) {
        print_hex(byte_ptr[i]);
        print(" ");
        
        if ((i + 1) % 16 == 0) {
            print("\n");
        }
    }
    print("\n\n");
}



void kmain(uint32_t magic, struct MultibootInfo* mbi_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) 
        return;
    
    if ((mbi_info->flags & (1 << 11)) == 0) 
        return;
    
    gdt_initiate();
    idt_initiate();
    
    //init_sse();
    
    timer_init();
    
    char keyboard_string[255];
    char prompt_string[255];
    char virtual_key_map[255];
    
    // Frame buffer
    
    draw_set_info((uint32_t)mbi_info);
    
    display_init();
    draw_set_clip_rect(0, 0, display_width, display_height);
    
    uint32_t fb_pixels = mbi_info->framebuffer_width * mbi_info->framebuffer_height;
    uint32_t fb_size_bytes = fb_pixels * sizeof(uint32_t);
    
    // Paging
    
    paging_initiate(mbi_info);
    page_allocator_init(mbi_info);
    
    // Lock the Graphics back buffer at the 8MB mark
    // This is well within our 64MB chunk
    uint32_t* forced_back_buffer = (uint32_t*)0x00800000;
    
    extern void draw_set_frame_buffer(uint32_t* buffer_ptr);
    draw_set_frame_buffer(forced_back_buffer);
    
    uint32_t heap_start = 0x01800000; 
    heap_set_base_address(heap_start);
    
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
    mouse_set_cursor_speed(14, 14);
    mouse_set_cursor_acceleration(2);
    
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    kernel_init();
    
    console_prompt_set_string("/>");
    print("kernel v0.0.0\n");
    draw_flush_display();
    
    pci_init();
    
    //debug_dump_storage_first_bytes();
    
    uint16_t io_base = 0x1F0;
    
    ata_init(io_base);
    
    uint8_t sector_data[512];
    //memset(sector_data, 0x55, 512);
    //ata_write_sector(io_base, 0, sector_data);
    
    ata_read_sector(io_base, 0, sector_data);
    
    uint8_t line=0;
    for (unsigned int i=0; i < 512; i++) {
        if (line > 32) {line=0; print("\n");}
        line++;
        
        print_hex(sector_data[i]);
        print(" ");
        
    }
    print("\n\n");
    draw_flush_display();
    
    
    //
    // Command console boot option
    
    {
    
    bool activate_console = false;
    draw_flush_display();
    
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
                    draw_flush_display();
                    break;
                }
                
                draw_flush_display();
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
                draw_flush_display();
            }
        }
    }
    
    }
    
    //
    // Initiate desktop environment
    
    dwm_initiate();
    
    desktop_environment_init();
    
    
    create_folder(30, 30, "system");
    create_folder(30, 130, "new folder");
    
    
    
    
    
    // Event system
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // Make context menus instantiable for sub menuing
    
    // Scalable vector font
    
    
    while(1) {
        
        dwm_update();
        
        
        
        //
        // Testing - rapid creation / deletion
        //
        /*
        WindowHandle parent;
        {
        WindowClass wclass;
        wclass.x = 0;
        wclass.y = 0;
        wclass.width = 65;
        wclass.height = 27;
        parent = create_window(wclass, WINDOW_STYLE_NOBORDERS, NULL);
        }
        
        if (parent == 0) 
            trigger_test_page_fault();
        {
        
        for (unsigned int i=0; i < 3; i++) {
            WindowClass wclass;
            wclass.x = 0;
            wclass.y = 0;
            wclass.width = 65;
            wclass.height = 27;
            WindowHandle child = create_window(wclass, WINDOW_STYLE_NOBORDERS, NULL);
            dwm_window_set_parent(child, parent);
        }
        }
        
        destroy_window(parent);
        */
        
        
        
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

