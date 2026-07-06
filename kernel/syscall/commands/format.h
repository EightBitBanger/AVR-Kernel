#ifndef SYSCALL_FORMAT_H
#define SYSCALL_FORMAT_H

#include <stdint.h>
#include <kernel/console/display.h>
#include <kernel/fs/fs.h>

// Static string constants extracted to the top of the file
static const char* msg_bytes_total       = " bytes total\n";
static const char* msg_sector_size       = " sector size\n\n";
static const char* msg_formatting        = "Formatting...\n";
static const char* msg_unmounted         = "Directory unmounted\n";
static const char* msg_unknown_arg       = "Unknown argument '";
static const char* msg_missing_val       = "Missing value for argument '";
static const char* msg_invalid_val       = "Invalid value for argument '";
static const char* msg_unspecified_param = "Parameters unspecified\n";
static const char* msg_quote_newline     = "'\n";
static const char* msg_percent           = "%";
static const char* msg_init_progress     = "0%";
static const char* msg_quick_progress    = "50%";
static const char* msg_final_progress    = "100%\n\n";

int call_routine_format(int arg_count, char** args) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    uint32_t total_capacity = 0;
    uint32_t sector_size    = 32UL;
    uint32_t device_address = fs_current.mount_device;
    uint8_t quick_format = 0;
    
    if (device_address == FS_NULL) {
        print(msg_unmounted);
        return 1;
    }
    
    for (int i = 0; i < arg_count; i++) {
        char* argument = args[i];
        
        // Ensure argument starts with '-' and has at least a flag character
        if (argument[0] != '-' || argument[1] == '\0') {
            print(msg_unknown_arg);
            print(argument);
            print(msg_quote_newline);
            return 2;
        }
        
        char flag_type = argument[1];
        
        // Quick format request (e.g., "-q")
        if (flag_type == 'q') {
            // Ensure there are no trailing characters after '-q'
            if (argument[2] != '\0') {
                print(msg_unknown_arg);
                print(argument);
                print(msg_quote_newline);
                return 2;
            }
            quick_format = 1;
        }
        // Handle flags that require numeric values (-k, -M, -s)
        else if (flag_type == 'k' || flag_type == 'M' || flag_type == 's') {
            // Ensure there is actually a value string following the flag character
            if (argument[2] == '\0') {
                print(msg_missing_val);
                print(argument);
                print(msg_quote_newline);
                return 2;
            }

            // Inlined parse_int logic
            uint32_t value = 0;
            const char* str = &argument[2];
            const char* startptr = str;
            
            while (*str >= '0' && *str <= '9') {
                value = value * 10 + (*str - '0');
                str++;
            }
            
            if (str == startptr || *str != '\0') {
                print(msg_unknown_arg);
                print(argument);
                print(msg_quote_newline);
                return 2;
            }
            
            // Handle Capacity Flags
            if (flag_type == 'k') {
                total_capacity = value * 1024UL;
            }
            else if (flag_type == 'M') {
                total_capacity = value * 1024UL * 1024UL;
            }
            // Handle Sector Flags
            else if (flag_type == 's' && (value == 32 || value == 64 || value == 512 || value == 1024)) {
                sector_size = value;
            }
            else {
                print(msg_invalid_val);
                print(argument);
                print(msg_quote_newline);
                return 2;
            }
        }
        // If it didn't match any valid flag type
        else {
            print(msg_unknown_arg);
            print(argument);
            print(msg_quote_newline);
            return 2;
        }
    }
    
    if (total_capacity == 0) {
        print(msg_unspecified_param);
        return 3;
    }
    
    // Print common header messages
    print(msg_formatting);
    print_int(total_capacity);
    print(msg_bytes_total);
    print_int(sector_size);
    print(msg_sector_size);
    
    if (!quick_format) {
        print(msg_init_progress);
        
        uint8_t last_percentage = 0;
        uint32_t bytes_per_percent = total_capacity / 100;
        
        for (uint32_t address_range = 0; address_range < total_capacity; address_range++) {
            if (bytes_per_percent > 0 && (address_range % bytes_per_percent == 0)) {
                uint32_t current_percentage = address_range / bytes_per_percent;
                
                if (current_percentage > last_percentage && current_percentage <= 100) {
                    last_percentage = (uint8_t)current_percentage;
                    
                    display_cursor_set_position(0);
                    print_int(current_percentage);
                    print(msg_percent);
                }
            }
            fs_writeb(address_range, 0x00);
        }
    } else {
        print(msg_quick_progress);
    }
    
    fs_device_format(device_address, total_capacity, sector_size, FS_DEVICE_TYPE_ATA);
    
    struct FSPartitionBlock partition;
    fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA);
    
    uint32_t root_directory = fs_directory_create("root", FS_PERMISSION_READ | FS_PERMISSION_WRITE, FS_NULL);
    
    partition.root_directory = root_directory;
    
    fs_mem_write(sizeof(struct FSDeviceHeader), &partition, sizeof(struct FSPartitionBlock));
    
    fs_cache_sync();
    fs_bitmap_flush();
    fs_current.mount_root = root_directory;
    
    kernel_set_working_directory(&fs_current);
    
    display_cursor_set_position(0);
    print(msg_final_progress);
    return 0;
}

#endif
