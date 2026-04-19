#ifndef SYSCALL_BOOT_H
#define SYSCALL_BOOT_H

#include <stdint.h>
#include <kernel/kernel.h>

#include <kernel/emulation/x4/x4.h>

uint8_t program[] = {
    0x89, 0x01, 0x03, 0xCD, 0x10, 0xCD, 0x20, 
};

int call_routine_boot(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    struct FSPartitionBlock part;
    fs_device_open(fs_current.mount_device, &part);
    
    uint32_t handle = fs_file_create("cls", FS_PERMISSION_READ | FS_PERMISSION_WRITE | FS_PERMISSION_EXECUTE, sizeof(program), fs_current.mount_directory);
    
    FileHandle file;
    if (fs_file_open(&file, handle, FS_FILE_MODE_WRITE) == true) {
        fs_file_write(&file, &program, sizeof(program));
        
        fs_file_close(&file);
    }
    
    fs_bitmap_flush();
    return 0;
    
    
    
    /*
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    char path[32];
    console_get_path(path, 32, fs_current.current_directory, fs_current.mount_directory, 2);
    
    print(path);
    print("\n");
    
    return 0;
    */
    
    
    // Runner example
    
    uint32_t code_segment  = kmalloc(sizeof(program));
    uint32_t stack_segment = kmalloc(1024);
    
    for (uint32_t i=0; i < sizeof(program); i++) 
        kmalloc_write(code_segment + i, &program[i], 1);
    
    struct X4Thread thread;
    memset(&thread, 0x00, sizeof(struct X4Thread));
    
    thread.cache.cs = code_segment;
    thread.cache.ss = stack_segment;
    
    thread.cache.ep = 0;
    
    x4_emulate(&thread, args, arg_count);
    
    kfree(thread.cache.cs);
    kfree(thread.cache.ss);
    
    return 0;
}

#endif
