#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>
#include <kernel/string.h>

#include <string.h>


static uint16_t task_index = 0;

void scheduler_init(void) {
    
}


uint32_t create_task(const char* name, uint32_t entry_point_address, uint16_t priority, uint8_t flags) {
    struct X4Thread thread;
    memset(&thread, 0x00, sizeof(struct X4Thread));
    
    thread.cache.cs = entry_point_address;
    thread.cache.ss = kmalloc(1024);
    
    // Create process node
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    // Find "proc" directory
    uint32_t proc_knode = knode_find_by_name(knode_get_root(), "proc");
    uint32_t proc_address = create_knode(name, proc_knode);
    
    // Add to "proc"
    uint32_t process_block_address = create_procblock("");
    knode_add_reference(proc_address, process_block_address);
    
    // Create the process block
    struct ProcessBlock block;
    memset(&block, 0x00, sizeof(struct ProcessBlock));
    strcpy(block.name, "block");
    
    block.code_segment_address  = thread.cache.cs;
    block.stack_segment_address = thread.cache.ss;
    
    kmem_write(process_block_address, &block, sizeof(struct ProcessBlock));
    task_index++;
}



uint8_t counter = 0;
uint32_t proc_root = KNODE_NULL;
uint16_t index=0;

void scheduler_handler(void) {
    counter++;
    if (counter < 50) 
        return;
    counter=0;
    
    if (proc_root == KNODE_NULL) {
        proc_root = knode_find_by_name(knode_get_root(), "proc");
        if (proc_root == KNODE_NULL) {
            index=0;
            return;
        }
    }
    
    uint32_t process_node = knode_get_reference(proc_root, index);
    if (process_node == KNODE_NULL) {
        index=0;
        return;
    }
    
    uint32_t process_block = knode_get_reference(process_node, 0);
    
    
    char name[16];
    knode_get_name(process_block, name);
    
    print(name);
    print("\n");
    
    
    //kmemcpy();
    
    //kmemcpy();
    //ProcessBlock
    
    
    
    
    
    // Run and free if program quit
    /*
    x4_emulate(&thread, (char*[]){NULL}, 0, 1000);
    
    kfree(thread.cache.ss);
    kfree(thread.cache.cs);
    */
    
    index++;
}















uint8_t execute_program(char* filename, char** args, uint8_t arg_count) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_directory == FS_NULL) 
        return 1;
    
    uint32_t address = fs_directory_find(fs_current.mount_directory, filename);
    if (address == FS_NULL) 
        return 2;
    
    uint8_t attributes, persmissions;
    fs_file_get_attributes(address, &attributes);
    fs_file_get_permissions(address, &persmissions);
    
    if (!(persmissions & FS_PERMISSION_EXECUTE) || attributes & FS_ATTRIBUTE_DIRECTORY) 
        return 3;
    
    uint32_t file_size = fs_file_get_size(address);
    
    FileHandle file;
    if (fs_file_open(&file, address, FS_FILE_MODE_READ) == true) {
        uint32_t code_segment  = kmalloc(file_size);
        uint32_t stack_segment = kmalloc(1024);
        
        char buffer;
        for (uint32_t i=0; i < file_size; i++) {
            fs_file_read(&file, &buffer, 1);
            kmem_write(code_segment + i, &buffer, 1);
        }
        
        struct X4Thread thread_main;
        memset(&thread_main, 0x00, sizeof(struct X4Thread));
        
        thread_main.cache.cs = code_segment;
        thread_main.cache.ss = stack_segment;
        
        x4_emulate(&thread_main, args, arg_count, 30);
        
        kfree(code_segment);
        kfree(stack_segment);
        
        fs_file_close(&file);
        return 0;
    }
    return 4;
}
