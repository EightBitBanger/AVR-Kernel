#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/kernel.h>

#include <kernel/emulation/x4/x4.h>
#include <kernel/fs/fs.h>
#include <kernel/scheduler/scheduler.h>


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
        uint32_t stack_segment = kmalloc(512);
        
        char buffer;
        for (uint32_t i=0; i < file_size; i++) {
            fs_file_read(&file, &buffer, 1);
            kmalloc_write(code_segment + i, &buffer, 1);
        }
        
        struct X4Thread thread;
        memset(&thread, 0x00, sizeof(struct X4Thread));
        
        thread.cache.cs = code_segment;
        thread.cache.ss = stack_segment;
        
        x4_emulate(&thread, args, arg_count);
        
        kfree(code_segment);
        kfree(stack_segment);
        
        fs_file_close(&file);
        return 0;
    }
    return 4;
}
