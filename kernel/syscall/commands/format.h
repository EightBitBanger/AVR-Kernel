#ifndef SYSCALL_FORMAT_H
#define SYSCALL_FORMAT_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_format(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    uint32_t total_capacity = 0;
    uint32_t sector_size    = 32UL;
    uint32_t device_address = fs_current.mount_device;
    
    if (device_address == FS_NULL) {
        print("Directory unmounted\n");
        return 1;
    }
    
    for (int i=0; i < arg_count; i++) {
        char* argument = args[i];
        
        if (strcmp(argument, "-4k" ) == 0) {total_capacity = 1024UL * 4UL; continue;}
        if (strcmp(argument, "-8k" ) == 0) {total_capacity = 1024UL * 8UL; continue;}
        if (strcmp(argument, "-16k") == 0) {total_capacity = 1024UL * 16UL; continue;}
        if (strcmp(argument, "-32k") == 0) {total_capacity = 1024UL * 32UL; continue;}
        
        if (strcmp(argument, "-32s")  == 0)  {sector_size = 32UL; continue;}
        if (strcmp(argument, "-64s")  == 0)  {sector_size = 64UL; continue;}
        if (strcmp(argument, "-512s") == 0)  {sector_size = 512UL; continue;}
        if (strcmp(argument, "-1024s") == 0) {sector_size = 1024UL; continue;}
        
        print("Unknown argument '");
        print(argument);
        print("'\n");
        return 2;
    }
    
    if (total_capacity == 0) {
        print("Parameters unspecified\n");
        return 3;
    }
    
    print("Formatting...\n");
    
    print_int(total_capacity);
    print(" bytes total\n");
    
    print_int(sector_size);
    print(" sector size\n");
    
    fs_device_format(device_address, total_capacity, sector_size);
    
    struct FSPartitionBlock partition;
    fs_device_open(device_address, &partition);
    
    uint32_t root_directory = fs_directory_create("root", FS_PERMISSION_READ | FS_PERMISSION_WRITE, FS_NULL);
    
    partition.root_directory = root_directory;
    
    fs_mem_write(sizeof(struct FSDeviceHeader), &partition, sizeof(struct FSPartitionBlock));
    
    fs_bitmap_flush();
    return 0;
}

#endif
