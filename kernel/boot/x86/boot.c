#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/slab.h>
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


extern char _kernel_program_end[];

#define PS2_STATUS_REG         0x64
#define PS2_DATA_REG           0x60
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_MOUSE_DATA  0x20

bool ps2_check_keyboard(void);
void flush_keyboard_buffer(void);
void ps2_route_console(void);

void vmm_stress_test(void);

void kmain(uint32_t magic, struct MultibootInfo* mbi) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) 
        return;
    
    if ((mbi->flags & (1 << 11)) == 0) 
        return;
    
    uint32_t framebuffer_pixels      = mbi->framebuffer_width * mbi->framebuffer_height;
    uint32_t framebuffer_size_bytes  = framebuffer_pixels * sizeof(uint32_t);
    
    // 16 byte aligned
    uint32_t heap_start              = ((uint32_t)_kernel_program_end + 0xFU) & ~0xFU;
    uint32_t heap_size               = 1024U * 1024U * 64U;
    uint32_t block_size              = 16U;
    
    // 4k page aligned
    uint32_t front_buffer            = (heap_start + heap_size + 0xFFFU) & ~0xFFFU;
    uint32_t back_buffer             = (front_buffer + framebuffer_size_bytes + 0xFFFU) & ~0xFFFU;
    
    uint32_t _kernel_memory_end      = (back_buffer + framebuffer_size_bytes + 0xFFFU) & ~0xFFFU;
    
    gdt_init();
    idt_init();
    
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
    pmm_init(mbi, _kernel_memory_end);
    vmm_init(mbi, _kernel_memory_end);
    
    // Map the graphics front frame buffer 
    // as a cached write combine buffer
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
    
    
    while(1) {
        
        /*
        // Get a raw page
        uint32_t* raw_page = (uint32_t*)vmm_alloc_pages(1);
        
        // Write a highly specific magic number to the very first byte
        *raw_page = 0xDEADBEEF;
        
        // Read it immediately back
        print("Wrote DEADBEEF, Read back: ");
        print_hex32(*raw_page);
        print("\n");
        
        draw_flush_display();
        while(1);
        */
        
        vmm_stress_test();
    }
    
    
    //
    // Command console boot options
    //
    
    {
        bool activate_console = false;
        
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
    
    // Blank the screen in preparation for the graphical environment
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), 0xFF000000);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    // Scan PCI bus for available hardware
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

SlabCache thread_cb_cache;

void vmm_stress_test(void) {
    thread_cb_cache.object_size = 64;
    thread_cb_cache.page_list = NULL;
    
    uint8_t* object = slab_alloc(&thread_cb_cache);
    
    print_hex32((uint32_t) object);
    print("\n");
    draw_flush_display();
    
    slab_free(&thread_cb_cache, object);
    
    return;
    
    
    while(1) {
        //uint32_t* page = vmm_alloc_pages(1);
        uint32_t frame = pmm_alloc_frame();
        
        print_hex32( (uint32_t)frame );
        
        print("\n");
        draw_flush_display();
    }
    
    while(1);
    
    
    
    
    /*
    thread_cb_cache.object_size = 64;
    thread_cb_cache.page_list = NULL;
    
    uint8_t* objA = slab_alloc(&thread_cb_cache);
    uint8_t* objB = slab_alloc(&thread_cb_cache);
    
    print_hex((uint32_t)thread_cb_cache.object_size);
    
    draw_flush_display();
    
    while(1);
    
    //uint8_t* objA = slab_alloc(&thread_cb_cache);
    //uint8_t* objB = slab_alloc(&thread_cb_cache);
    //uint8_t* objC = slab_alloc(&thread_cb_cache);
    
    
    print_hex((uint32_t) objA);
    print("\n");
    print_hex((uint32_t) objB);
    print("\n");
    print_hex((uint32_t) objC);
    print("\n");
    */
    
    while(1);
    
    //slab_free(&thread_cb_cache, obj);
    
    //while(1);
    
    //void* obj1 = slab_alloc(&thread_cb_cache); 
    //void* obj2 = slab_alloc(&thread_cb_cache);
    
    //slab_free(&thread_cb_cache, obj1); 
    //slab_free(&thread_cb_cache, obj2);
    /*
    uint8_t* pages[5];
    pages[0] = vmm_alloc_pages(1);
    pages[1] = vmm_alloc_pages(1);
    pages[2] = vmm_alloc_pages(1);
    pages[3] = vmm_alloc_pages(1);
    pages[4] = vmm_alloc_pages(1);
    
    print_hex32( (uint32_t) pages[1] );
    
    //vmm_free_pages(pages[0], 1);
    vmm_free_pages(pages[1], 1);
    //vmm_free_pages(pages[2], 1);
    vmm_free_pages(pages[3], 1);
    //vmm_free_pages(pages[4], 1);
    
    print("\n");
    pages[1] = vmm_alloc_pages(10);
    
    print_hex32( (uint32_t) pages[1] );
    
    vmm_free_pages(pages[0], 1);
    vmm_free_pages(pages[1], 10);
    vmm_free_pages(pages[2], 1);
    vmm_free_pages(pages[4], 1);
    
    print("\n");
    
    print("\n");
    draw_flush_display();
    
    */
}




