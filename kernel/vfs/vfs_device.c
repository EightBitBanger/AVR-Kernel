#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

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
    if (path == NULL || path[0] == '\0' || path[0] == ' ') 
        return false;
    
    // Resolve the target file/directory's current internal address
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false; // Target item does not exist
    
    if (knode_check_is_valid(address)) {
        uint32_t device_address = knode_get_reference(address, 0);
        
        struct FSPartitionBlock part;
        if (fs_device_open(address, &part, FS_DEVICE_TYPE_ATA) != 0) {
            if (part.magic == FS_MAGIC) 
                return part.total_size;
        }
    }
    return 0;
}

uint64_t vfs_device_get_used(const char* path) {
    return fs_get_used_bytes();
}
