#ifndef SYSCALL_FORMAT_H
#define SYSCALL_FORMAT_H

#include <stdint.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>

int call_routine_format(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    uint32_t total_capacity = 0;
    uint32_t sector_size    = 32UL;
    uint32_t device_address = fs_current.mount_device;
    
    if (device_address == FS_NULL) {
        print("Directory unmounted\n");
        return 1;
    }
    
    for (int i = 0; i < arg_count; i++) {
        char* argument = args[i];
        
        // Ensure argument starts with '-' and has at least a flag character and a digit
        if (argument[0] != '-' || argument[1] == '\0' || argument[2] == '\0') {
            print("Unknown argument '");
            print(argument);
            print("'\n");
            return 2;
        }
        
        char flag_type = argument[1];
        
        // Inlined parse_int logic
        uint32_t value = 0;
        const char* str = &argument[2];
        const char* startptr = str; // Track the start to ensure at least one digit is parsed
        
        while (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
            str++;
        }
        
        // If no digits were parsed or there are extra characters at the end, it's invalid
        if (str == startptr || *str != '\0') {
            print("Unknown argument '");
            print(argument);
            print("'\n");
            return 2;
        }
        
        // Handle Capacity Flags (-k32, -M1, etc.)
        if (flag_type == 'k') {
            total_capacity = value * 1024UL;
        }
        else if (flag_type == 'M') {
            total_capacity = value * 1024UL * 1024UL;
        }
        // Handle Sector Flags (-s32, -s512, etc.)
        else if (flag_type == 's' && (value == 32 || value == 64 || value == 512 || value == 1024)) {
            sector_size = value;
        }
        // If it didn't match any valid conditions:
        else {
            print("Unknown argument '");
            print(argument);
            print("'\n");
            return 2;
        }
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
    
    fs_device_format(device_address, total_capacity, sector_size, FS_DEVICE_TYPE_ATA);
    
    struct FSPartitionBlock partition;
    fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA);
    
    uint32_t root_directory = fs_directory_create("root", FS_PERMISSION_READ | FS_PERMISSION_WRITE, FS_NULL);
    
    partition.root_directory = root_directory;
    
    fs_mem_write(sizeof(struct FSDeviceHeader), &partition, sizeof(struct FSPartitionBlock));
    
    fs_bitmap_flush();
    fs_current.mount_root = root_directory;
    
    kernel_set_working_directory(&fs_current);
    return 0;
}

#endif
