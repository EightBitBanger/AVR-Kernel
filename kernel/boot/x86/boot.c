#include <stddef.h>
#include <stdint.h>

#include <kernel/arch/x86/heap.h>

#include <kernel/kernel.h>

#include <kernel/console/print.h>
#include <kernel/console/display.h>
#include <kernel/console/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/console/virtual_key.h>

extern char _kernel_end[];

void kmain(void) {
    char keyboard_string[255];
    char prompt_string[255];
    char virtual_key_map[255];
    
    // Display and keyboard for the console
    
    display_init();
    display_clear();
    
    display_cursor_set_line(0);
    display_cursor_set_position(0);
    
    kb_init();
    kb_map_init(virtual_key_map, sizeof(virtual_key_map));
    
    console_init(keyboard_string, prompt_string, sizeof(keyboard_string), sizeof(prompt_string));
    
    // Heap allocator
    
    uint32_t heap_sz  = ((uint32_t)1024 * (uint32_t)1024 * (uint32_t)1024 * 4) - 1;
    uint32_t block_sz = 64;
    
    uint32_t heap_start = ((uint32_t)_kernel_end + 4095) & ~4095;
    heap_set_base_address(heap_start);
    
    heap_init(block_sz, heap_sz);
    
    kernel_init();
    
    console_prompt_set_string("/>");
    print("kernel v0.0.0\n");
    
    console_prompt_print();
    while(1) {
        
        kb_event_handler();
    }
    
}
