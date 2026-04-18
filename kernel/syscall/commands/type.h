#ifndef SYSCALL_TYPE_H
#define SYSCALL_TYPE_H

#include <stdint.h>
#include <kernel/kernel.h>

int call_routine_type(int arg_count, char** args) {
    if (arg_count == 0) 
        return 1;
    
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_directory == FS_NULL) 
        return 1;
    
    uint32_t address = fs_directory_find(fs_current.mount_directory, args[0]);
    if (address == FS_NULL) 
        return 2;
    
    FileHandle file;
    if (fs_file_open(&file, address, FS_FILE_MODE_READ) == true) {
        uint32_t file_size = fs_file_get_size(address);
        
        for (uint32_t i=0; i < file_size; i++) {
            char ch[2] = {0, '\0'};
            fs_file_read(&file, &ch[0], 1);
            
            if (ch[0] == '\0') 
                break;
            
            print(ch);
        }
        
        fs_file_close(&file);
    }
    return 0;
}

#endif
