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
    
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) {
        return false;
    }
    
    // Check if it's a raw virtual memory node (Knode space)
    if (kmalloc_is_valid(address)) {
        uint8_t k_flags = kmalloc_get_flags(address);
        // It IS a directory if either the directory flag OR mount flag is set
        if (k_flags & (KMALLOC_FLAG_MOUNT | KMALLOC_FLAG_DIRECTORY)) {
            return true;
        }
    }
    
    // Check if it's an underlying physical file system node
    if (fs_check_directory_valid(address)) {
        return true;
    }
    
    return false;
}

bool vfs_directory_check(const char* path) {
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
    
    if (fs_check_directory_valid(address)) {
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
    
    if (fs_check_directory_valid(address)) {
        uint32_t parent = fs_directory_get_parent(address);
        if (parent == address || parent == FS_NULL) { 
            return true;
        }
    }
    
    return false;
}

uint32_t vfs_directory_get_count(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    if (fs_check_directory_valid(address)) {
        return fs_directory_get_reference_count(address);
    } else {
        return knode_get_reference_count(address);
    }
    return 0;
}
