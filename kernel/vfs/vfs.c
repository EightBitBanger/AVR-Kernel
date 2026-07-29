#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs.h>
#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

struct list_node* open_files_head = NULL;
struct list_node* open_files_tail = NULL;
File next_unique_id = 1;

bool vfs_is_directory(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    if (vfs_directory_check_mounted(path)) 
        return true;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    
    if (kmalloc_is_valid(address)) {
        uint8_t k_flags = kmalloc_get_flags(address);
        if (k_flags & (KMALLOC_FLAG_MOUNT | KMALLOC_FLAG_DIRECTORY)) 
            return true;
    }
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, address)) 
        return true;
    
    return false;
}

bool vfs_directory_check(const char* path) {
    return vfs_is_directory(path);
    
    if (path == NULL || path[0] == '\0') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    if (kmalloc_is_valid(address)) {
        uint8_t k_flags = kmalloc_get_flags(address);
        if (k_flags & KMALLOC_FLAG_DIRECTORY) {
            return true;
        }
    }
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, address)) {
        return true;
    }
    
    return false;
}

bool vfs_directory_check_mounted(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    if (kmalloc_is_valid(address)) {
        uint8_t k_flags = kmalloc_get_flags(address);
        if (k_flags & KMALLOC_FLAG_MOUNT) {
            return true;
        }
    }
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, address)) {
        uint32_t parent = fs_directory_get_parent(ctx, address);
        if (parent == address || parent == FS_NULL) { 
            return true;
        }
    }
    
    return false;
}

uint32_t vfs_directory_get_item_count(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return 0;
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return 0;
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, address)) {
        return fs_directory_get_reference_count(ctx, address);
    } else {
        return knode_get_reference_count(address);
    }
    return 0;
}

bool vfs_directory_get_item(const char* path, unsigned int index, char* name_out) {
    if (path == NULL || path[0] == '\0') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, address)) {
        uint32_t item_address = fs_directory_get_reference(ctx, address, index);
        fs_file_get_name(ctx, item_address, name_out);
        return true;
    } else {
        uint32_t item_address = knode_get_reference(address, index);
        knode_get_name(item_address, name_out);
        return true;
    }
    return false;
}
