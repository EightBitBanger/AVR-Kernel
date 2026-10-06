#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

//
// Internal helpers (caller holds the VFS lock)
//

struct FSDeviceContext* vfs_device_get_context(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return NULL;
    if (knode_check_is_valid(address) == 0) 
        return NULL;
    
    // Check if the node is actually a mounted filesystem
    uint8_t flags = kmalloc_get_flags(address);
    if ((flags & KMALLOC_FLAG_MOUNT) == 0) {
        return NULL;
    }
    
    return (struct FSDeviceContext*)knode_get_reference(address, 1);
}

uint8_t* vfs_device_get_block(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return NULL;
    if (knode_check_is_valid(address) == 0) 
        return NULL;
    
    // Check if the node is actually a mounted filesystem
    uint8_t flags = kmalloc_get_flags(address);
    if ((flags & KMALLOC_FLAG_MOUNT) == 0) {
        return NULL;
    }
    
    return (uint8_t*)knode_get_reference(address, 0);
}

// The returned pointer is only safe to use while the caller holds the VFS
// lock. The lock taken here (recursively) just guards the walk itself.
OpenFileDescriptor* vfs_file_find_open(File id) {
    if (id == VFS_INVALID_FILE) return NULL;
    
    vfs_lock();
    OpenFileDescriptor* found = NULL;
    struct list_node* current = open_files_head;
    while (current != NULL) {
        OpenFileDescriptor* desc = (OpenFileDescriptor*)current->data;
        if (desc->id == id) {
            found = desc;
            break;
        }
        current = current->next;
    }
    vfs_unlock();
    return found;
}

//
// Implementations (caller holds the VFS lock)
//

static uint64_t do_device_get_capacity(const char* path) {
    uint32_t address = resolve_path_to_mount_point(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return 0;
    if (knode_check_is_valid(address) == 0) 
        return 0;
    
    struct FSDeviceContext* device_context = (struct FSDeviceContext*)knode_get_reference(address, 1);
    if (!device_context) return 0;

    return device_context->pool_size;
}

static uint64_t do_device_get_used(const char* path) {
    struct FSDeviceContext* device_context = vfs_device_get_context(path);
    if (!device_context) return 0;
    return fs_get_used_bytes(device_context);
}

static bool do_sync_all(void) {
    uint32_t root = knode_get_root();
    uint32_t mnt  = knode_find_by_name(root, "mnt");
    if (mnt == KNODE_NULL || mnt == 0)
        return true;   // No mount directory: nothing to flush
    
    bool ok = true;
    uint32_t count = knode_get_reference_count(mnt);
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t mount = knode_get_reference(mnt, i);
        if (mount == KNODE_NULL || mount == 0)
            continue;
        if ((kmalloc_get_flags(mount) & KMALLOC_FLAG_MOUNT) == 0)
            continue;
        
        // Reference 1 of a mount knode is its FSDeviceContext (see pci.c)
        uint32_t ctx_ref = knode_get_reference(mount, 1);
        if (ctx_ref == KMALLOC_NULL || ctx_ref == 0)
            continue;
        
        struct FSDeviceContext* ctx = (struct FSDeviceContext*)ctx_ref;
        
        // An unformatted device was never written through, so it has
        // nothing pending; skip it rather than poke the hardware
        if (!ctx->is_open)
            continue;
        
        if (!fs_device_sync(ctx))
            ok = false;
    }
    
    return ok;
}

//
// Public API
//

uint64_t vfs_device_get_capacity(const char* path) {
    vfs_lock();
    uint64_t result = do_device_get_capacity(path);
    vfs_unlock();
    return result;
}

uint64_t vfs_device_get_used(const char* path) {
    vfs_lock();
    uint64_t result = do_device_get_used(path);
    vfs_unlock();
    return result;
}

bool vfs_sync_all(void) {
    vfs_lock();
    bool result = do_sync_all();
    vfs_unlock();
    return result;
}
