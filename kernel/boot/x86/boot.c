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
#include <kernel/arch/x86/drivers/rng.h>

// Utility
#include <kernel/util/math.h>
#include <kernel/util/string.h>
#include <kernel/util/timer.h>
#include <kernel/util/random.h>

// Console
#include <kernel/console/print.h>
#include <kernel/console/console.h>
#include <kernel/console/virtual_key.h>

#include <kernel/registry/registry.h>
#include <kernel/dwm/dwm.h>
#include <kernel/panic/panic_error.h>
#include <kernel/scheduler/scheduler.h>

#define BOOT_DELAY_MS  500

// Position in memory where the kernel ends
extern char _kernel_program_end[];

void run_allocator_stress_test(void);


void thread_dwm_main(void) {
    while (1) {
        dwm_update();
        kernel_event_update();
        
        thread_yield();
    }
}

void dummy_runner(void) {
    while(1){
        
        
        //thread_yield();
        thread_sleep(100);
    }
}

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
    __asm__ __volatile__("sti");
    
    // Paging
    pmm_init(mbi, _kernel_memory_end);
    vmm_init(mbi, _kernel_memory_end);
    
    // Random number generation
    rand_init();
    
    // Initialize the kernels personal heap block
    heap_set_base_address(heap_start);
    heap_init(block_size, heap_size);
    
    // Fire up the scheduler
    scheduler_init();
    
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
            if (kb_get_current_char() == 'c') {
                activate_console = true;
                
                // Scan PCI bus for available hardware
                // before entering the console
                pci_init();
                
                kb_flush();
                
                console_prompt_print();
                draw_flush_display();
                break;
            }
        }
        
        // Run the dedicated console mode if activated
        while (activate_console) {
            ps2_route_console();
        }
    }
    
    // Scan PCI bus for available hardware
    pci_init();
    
    // Get primary knode directories
    uint32_t root_node = knode_get_root();
    uint32_t dev_directory = knode_find_by_name(root_node, "dev");
    uint32_t mnt_directory = knode_find_by_name(root_node, "mnt");
    uint32_t pci_directory = create_knode("pci", dev_directory);
    
    // Courtesy delay for boot output
    uint64_t old_ms = timer_get_ms();
    while ((timer_get_ms() - old_ms) <= BOOT_DELAY_MS);
    
    // Blank the screen in preparation for pure graphics mode
    draw_rect_filled(0, 0, display_get_width(), display_get_height(), 0xFF000000);
    draw_flush_region(0, 0, display_get_width(), display_get_height());
    
    //
    // Find the home directory
    
    const char* path_mnt = "/mnt";
    const char* path_sys = "/sys";
    
    char home_path[128];
    memset(home_path, '\0', sizeof(home_path));
    uint32_t item_count = vfs_directory_get_item_count(path_mnt);
    for (unsigned int i=0; i < item_count; i++) {
        char temp_path[128];
        strncpy(temp_path, path_mnt, 128);
        
        char item_name[16];
        if (!vfs_directory_get_item(path_mnt, i, item_name)) 
            continue;
        
        strncat(temp_path, "/", 128);
        strncat(temp_path, item_name, 128);
        
        strncat(temp_path, path_sys, 128);
        
        if (vfs_directory_check(temp_path)) {
            //print(temp_path);
            //print("\n");
        }
    }
    
    
    //
    // Load the registry
    
    registry_hive_initiate("/mnt/ata0/sys");
    
    //
    // Initiate the DWM graphical environment
    
    dwm_initiate();
    
    uint16_t sep = 90;
    uint16_t posx = 30;
    uint16_t posy = 30;
    
    //
    // Load desktop icons
    
    // Get mounted devices
    uint32_t number_of_mounts = knode_get_reference_count(mnt_directory);
    for (unsigned int i=0; i < number_of_mounts; i++) {
        uint32_t address = knode_get_reference(mnt_directory, i);
        
        char name[16];
        knode_get_name(address, name);
        
        char path[128];
        strncpy(path, path_mnt, 128);
        strncat(path, "/", 128);
        strncat(path, name, 128);
        
        // Find a system partition
        char path_system[128];
        strncpy(path_system, path, 128);
        strncat(path_system, path_sys, 128);
        
        if (vfs_directory_check(path_system)) {
            
            dwm_create_mount(posx, posy, name, path); posx += sep;
        }
        
        
    }
    
    
    //
    // TODOs
    
    // User event call back messaging
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    
    // TODO scalable vector font or MSDF
    
    // TODO color themes to handle color
    
    // TODO key combination binding
    
    //detach();
    
    for (unsigned int i=0; i < 24; i++) 
        thread_create(dummy_runner, PRIORITY_IDLE);
    
    thread_create(thread_dwm_main, PRIORITY_NORMAL);
    
    // Kernel main thread
    while(1) {
        
        
        thread_yield();
    }
}
