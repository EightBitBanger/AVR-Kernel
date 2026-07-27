#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

struct FSDeviceContext* vfs_device_get_context(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    if (knode_check_is_valid(address) == 0) 
        return 0;
    return (struct FSDeviceContext*)knode_get_reference(address, 1);
};

uint8_t* vfs_device_get_block(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    if (knode_check_is_valid(address) == 0) 
        return 0;
    return (uint8_t*)knode_get_reference(address, 0);
};

OpenFileDescriptor* vfs_file_find_open(File id) {
    if (id == VFS_INVALID_FILE) return NULL;
    
    struct list_node* current = open_files_head;
    while (current != NULL) {
        OpenFileDescriptor* desc = (OpenFileDescriptor*)current->data;
        if (desc->id == id) {
            return desc;
        }
        current = current->next;
    }
    return NULL;
}

uint64_t vfs_device_get_capacity(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    if (knode_check_is_valid(address) == 0) 
        return 0;
    
    uint32_t block_device = knode_get_reference(address, 0);
    struct FSDeviceContext* device_context = (struct FSDeviceContext*)knode_get_reference(address, 1);
    
    struct FSPartitionBlock part;
    fs_device_open(block_device, &part, device_context->device_type);
    
    return part.total_size;
}

uint64_t vfs_device_get_used(const char* path) {
    struct FSDeviceContext* device_context = vfs_device_get_context(path);
    return fs_get_used_bytes(device_context);
}
