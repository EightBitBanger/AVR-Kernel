#ifndef SYSCALL_LS_H
#define SYSCALL_LS_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_lsdir(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    if (fs_current.mount_device != FS_NULL) {
        console_print_fs_entry(fs_current.current_directory);
        return 0;
    }
    
    for (uint32_t reference_index=0;;reference_index++) {
        uint32_t reference_address = knode_get_reference(fs_current.current_directory, reference_index);
        if (reference_address == KMALLOC_NULL) 
            break;
        
        console_print_reference_entry(reference_address);
    }
    return 0;
}

#endif
