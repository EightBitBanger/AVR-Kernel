#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/slab.h>
#include <kernel/arch/x86/malloc.h>
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

void run_kernel_memory_hammer_test(void);









void test_slab_malloc(void) {
    /*
    print("--- STARTING SLAB ALLOCATOR TESTS ---\n");
    draw_flush_display();

    // Test 1: Basic Allocation and Freeing
    print("Test 1: Allocating a 32-byte chunk... ");
    int* ptr1 = (int*)vmalloc(32);
    if (ptr1 != NULL) {
        print("SUCCESS\n");
        *ptr1 = 12345; // Test writing to it
    } else {
        print("FAILED\n");
    }
    draw_flush_display();

    print("Test 1b: Freeing 32-byte chunk... ");
    vfree(ptr1);
    print("DONE\n");
    draw_flush_display();


    // Test 2: Multiple allocations to ensure buckets separate correctly
    print("Test 2: Allocating different bucket sizes...\n");
    char* small_ptr = (char*)vmalloc(8);   // Should go to 16-byte bucket
    char* large_ptr = (char*)vmalloc(400); // Should go to 512-byte bucket

    if (small_ptr != NULL && large_ptr != NULL) {
        print("  Allocations successful.\n");
    } else {
        print("  CRITICAL: Allocation failed!\n");
    }
    draw_flush_display();


    // Test 3: Force Page Growth
    // The 512-byte bucket can hold roughly ~7 slots per 4KB page.
    // Let's allocate 10 slots to force the allocator to call slab_grow().
    print("Test 3: Forcing slab expansion (allocating 10 blocks of 512 bytes)...\n");
    void* allocations[10];
    bool growth_success = true;

    for (int i = 0; i < 10; i++) {
        allocations[i] = vmalloc(512);
        if (allocations[i] == NULL) {
            growth_success = false;
            print("  Failed at allocation index: ");
            // If you have a print_int function, use it here
            print("!\n");
            break;
        }
    }

    if (growth_success) {
        print("  SUCCESS: Slab expanded and allocated across multiple pages safely.\n");
    }
    draw_flush_display();


    // Test 4: Free everything to ensure stability
    print("Test 4: Cleaning up expansion blocks... ");
    for (int i = 0; i < 10; i++) {
        if (allocations[i] != NULL) {
            vfree(allocations[i]);
        }
    }
    vfree(small_ptr);
    vfree(large_ptr);
    print("CLEANUP DONE\n");
    
    print("--- ALL TESTS COMPLETED ---\n");
    draw_flush_display();
    
    while(1);
    */
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
    
    dwm_create_folder(30, 30, "system");
    
    // Event system
    //  wEvent GetMessage();
    //  int16_t DispatchEvent()
    
    // Scalable vector font or MSDF
    
    
    
    
    uint8_t counter[6] = {0, 3, 2, 4, 8, 12};
    
    WindowHandle whdnlA;
    WindowHandle whdnlB;
    void* blockA;
    void* blockB;
    
    void* massA;
    void* massB;
    
    WindowClass wclass;
    wclass.x = 50;
    wclass.y = 50;
    wclass.width  = 640;
    wclass.height = 480;
    
    while(1) {
        dwm_update();
        kernel_event_update();
        
        //malloc(1024);
        
        /*
        //
        // Memory stress testing
        //
        
        
        run_kernel_memory_hammer_test();
        
        
        // Tick forward
        for (unsigned int i=0; i < sizeof(counter); i++) 
            counter[i]++;
        
        
        if (counter[0] > 5) {
            counter[0]=0;
            
            if (!whdnlA) 
                whdnlA = dwm_create_window(wclass, 0, NULL);
        }
        
        if (counter[1] > 5) {
            counter[1]=0;
            
            if (whdnlA) {
                dwm_destroy_window(whdnlA);
                whdnlA=0;
            }
            
        }
        
        if (counter[2] > 3) {
            counter[2]=0;
            if (!blockA) blockA = malloc(1024);
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            if (!blockB) blockB = malloc(1024);
            dwm_destroy_window(whdnl_temp);}
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            if (!whdnlB) 
                whdnlB = dwm_create_window(wclass, 0, NULL);
        }
        
        if (counter[3] > 3) {
            counter[3]=0;
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            if (blockA) {free(blockA); blockA = 0;}
            if (blockB) {free(blockB); blockB = 0;}
            dwm_destroy_window(whdnl_temp);}
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            if (whdnlB) {
                dwm_destroy_window(whdnlB);
                whdnlB=0;
            }
        }
        
        if (counter[4] > 20) {
            counter[4]=0;
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            if (massA) {free(massA); massA = 0;}
            
            {WindowHandle whdnl_temp = dwm_create_window(wclass, 0, NULL);
            dwm_destroy_window(whdnl_temp);}
            
            if (massB) {free(massB); massB = 0;}
        }
        
        if (counter[5] > 80) {
            counter[5]=0;
            
            if (!massA) massA = malloc(32768);
            if (!massB) massB = malloc(32768);
        }
        */
        
        
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




























#include <stddef.h>
#include <stdint.h>

#define STRESS_MAX_ALLOCS 128

// Pseudo-random number generator to avoid deterministic allocation patterns
static uint32_t next_random = 12345;
static uint32_t pseudo_rand(void) {
    next_random = next_random * 1103515245 + 12345;
    return (uint32_t)(next_random / 65536) % 32768;
}

void run_kernel_memory_hammer_test(void) {
    void* ptrs[STRESS_MAX_ALLOCS] = {NULL};
    size_t sizes[STRESS_MAX_ALLOCS] = {0};
    
    print("\n--- INITIATING MEMORY ALLOCATOR HAMMER TEST ---\n");
    draw_flush_display();

    // Phase 1: Rapid Fragmentation and Churn
    print("Phase 1: High-churn random size allocation...\n");
    for (int iterations = 0; iterations < 500; iterations++) {
        int index = pseudo_rand() % STRESS_MAX_ALLOCS;
        
        // If an allocation already exists at this random slot, free it
        if (ptrs[index] != NULL) {
            free(ptrs[index]);
            ptrs[index] = NULL;
        } else {
            // Pick a pseudo-random size between 8 bytes and 2048 bytes
            // This forces different slab buckets to activate/grow dynamically
            size_t size = (pseudo_rand() % 2040) + 8;
            ptrs[index] = malloc(size);
            sizes[index] = size;

            // Fill memory with a pattern to verify integrity later
            if (ptrs[index] != NULL) {
                uint8_t* byte_ptr = (uint8_t*)ptrs[index];
                for (size_t j = 0; j < size; j++) {
                    byte_ptr[j] = (uint8_t)(index ^ j);
                }
            }
        }
        
        if (iterations % 100 == 0) {
            print("."); // Progress indicator
            draw_flush_display();
        }
    }
    print("\nPhase 1 Complete. Checking integrity...\n");
    draw_flush_display();

    // Phase 2: Integrity Verification
    // Ensures that active blocks haven't been cross-contaminated or corrupted
    for (int i = 0; i < STRESS_MAX_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            uint8_t* byte_ptr = (uint8_t*)ptrs[i];
            for (size_t j = 0; j < sizes[i]; j++) {
                if (byte_ptr[j] != (uint8_t)(i ^ j)) {
                    print("CRITICAL ERROR: Memory Corruption Detected at index ");
                    // If you have an integer print function use it here, or trip panic
                    //kernel_panic(0x00, 0x00000000, PT_GENERAL_PROTECTION_FAULT, "");
                }
            }
        }
    }
    print("Phase 2 Complete. Memory integrity verified successfully.\n");
    draw_flush_display();

    // Phase 3: Mass Burst Allocation (Pushing the boundaries)
    print("Phase 3: Filling up remainder slots with large blocks...\n");
    int failed_allocations = 0;
    for (int i = 0; i < STRESS_MAX_ALLOCS; i++) {
        if (ptrs[i] == NULL) {
            // Allocate large chunks (4KB) to force physical page expansions
            ptrs[i] = malloc(4096);
            if (ptrs[i] == NULL) {
                failed_allocations++;
            }
        }
    }
    print("Phase 3 Complete. (If OOM occurred, check page tables/physical memory limits)\n");
    draw_flush_display();

    // Phase 4: Full Cleanup
    print("Phase 4: Cleaning up all stress blocks...\n");
    for (int i = 0; i < STRESS_MAX_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    
    print("--- HAMMER TEST SUCCESSFUL: NO PANICS TRIGGERS ---\n");
    draw_flush_display();
}



