#ifndef SYSCALL_FORMAT_H
#define SYSCALL_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/knode.h>
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
    uint32_t sector_size    = 512UL; // Default to standard 512-byte sector size
    uint32_t device_address = fs_current.mount_device;
    uint8_t quick_format    = 0;
    uint16_t device_type    = FS_DEVICE_TYPE_ATA;
    
    if (device_address == FS_NULL || device_address == 0) {
        print(msg_unmounted);
        return 1;
    }
    
    // Retrieve active mount context pointer to detect device type (AHCI vs ATA)
    //
    // Every context on a device shares its sector buffer (device_address), so
    // all reads and writes go through the mount's cached context. A temporary
    // context would leave the cached one with a stale sector_frame.
    struct FSDeviceContext* active_ctx = (struct FSDeviceContext*)knode_get_reference(fs_current.current_directory, 1);
    if (active_ctx == NULL || (uint32_t)active_ctx == KMALLOC_NULL) {
        print(msg_unmounted);
        return 1;
    }
    device_type = active_ctx->device_type;
    
    // Probe existing device partition block
    struct FSPartitionBlock existing_part;
    if (fs_device_get_partition(active_ctx, &existing_part) == 0) {
        if (existing_part.total_size > 0) {
            total_capacity = existing_part.total_size;
        }
        if (existing_part.sector_size > 0) {
            sector_size = existing_part.sector_size;
        }
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
            if (argument[2] == '\0') {
                print(msg_missing_val);
                print(argument);
                print(msg_quote_newline);
                return 2;
            }

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
    
    // Commit anything pending on the old filesystem before overwriting it
    fs_cache_sync(active_ctx);
    fs_bitmap_flush(active_ctx);
    active_ctx->frame_offset = FS_INVALID_FRAME;
    active_ctx->frame_dirty  = false;
    
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
            fs_writeb(active_ctx, address_range, 0x00);
        }
        fs_cache_sync(active_ctx);
    } else {
        print(msg_quick_progress);
    }
    
    // Format device low-level headers & bitmap structures using the active controller type.
    // fs_device_format() opens its own contexts on the shared buffer, so the
    // cached context is refreshed in place afterwards.
    fs_device_format(device_address, total_capacity, sector_size, device_type);
    
    struct FSPartitionBlock partition;
    *active_ctx = fs_device_open(device_address, &partition, device_type);
    if (!active_ctx->is_open) {
        print("Format failed\n");
        return 4;
    }
    
    // Create root directory with context reference
    uint32_t root_directory = fs_directory_create(active_ctx, "root", FS_PERMISSION_READ | FS_PERMISSION_WRITE, FS_NULL);
    
    partition.root_directory = root_directory;
    
    // Commit updated partition header to storage
    fs_mem_write(active_ctx, sizeof(struct FSDeviceHeader), &partition, sizeof(struct FSPartitionBlock));
    
    // Synchronize filesystem cache and bitmap
    fs_bitmap_flush(active_ctx);
    fs_cache_sync(active_ctx);
    
    // Update active working directory (the old directory addresses are gone)
    fs_current.mount_root      = root_directory;
    fs_current.mount_directory = root_directory;
    kernel_set_working_directory(&fs_current);
    
    display_cursor_set_position(0);
    print(msg_final_progress);
    
    return 0;
}

#endif

