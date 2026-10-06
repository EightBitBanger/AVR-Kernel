
#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

// VFS_NAME_MAX is the public name limit; it must leave room for the
// terminator inside the on-disk name field. (Array size goes negative,
// failing the build, if the two ever drift apart.)
typedef char vfs_name_max_matches_fs[(VFS_NAME_MAX == FS_NAME_LENGTH_MAX - 1) ? 1 : -1];

// A name is valid if it is 1..VFS_NAME_MAX characters, does not start with a
// space, and contains no '/' (which would split it into two path segments).
static bool vfs_name_is_valid(const char* name) {
    if (name == NULL || name[0] == '\0' || name[0] == ' ')
        return false;
    size_t length = strnlen(name, VFS_NAME_MAX + 1);
    if (length > VFS_NAME_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        if (name[i] == '/')
            return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return false;
    return true;
}

//
// Implementations (caller holds the VFS lock)
//
// Each public vfs_* function below takes the lock once and calls one of
// these, so a whole operation (resolve -> check -> modify) is atomic and an
// OpenFileDescriptor cannot be freed by vfs_close() while it is in use.
//

static File do_open(const char* path, uint16_t flags) {
    if (path == NULL || path[0] == '\0') 
        return VFS_INVALID_FILE;
    uint32_t current_knode = 0;
    uint32_t current_fs_node = 0;
    bool in_file_system = false;
    struct FSDeviceContext* ctx = NULL;

    if (!vfs_parse_path(path, flags, &current_knode, &current_fs_node, &in_file_system, &ctx)) {
        return VFS_INVALID_FILE;
    }
    
    if (in_file_system) {
        if (!fs_file_check(ctx, current_fs_node)) {
            return VFS_INVALID_FILE;
        }
    } else {
        uint8_t k_flags = kmalloc_get_flags(current_knode);
        if (k_flags & KMALLOC_FLAG_DIRECTORY) {
            return VFS_INVALID_FILE;
        }
    }
    
    OpenFileDescriptor* desc = (OpenFileDescriptor*)malloc(sizeof(OpenFileDescriptor));
    if (!desc) 
        return VFS_INVALID_FILE;

    desc->id = next_unique_id++;
    if (desc->id == VFS_INVALID_FILE) desc->id = next_unique_id++;  // skip 0 on wrap
    
    desc->in_file_system = in_file_system;
    desc->address = in_file_system ? current_fs_node : current_knode;
    desc->offset = 0;
    desc->flags = flags;
    desc->ctx = ctx;

    if (desc->in_file_system) {
        uint8_t perm = 0;
        fs_file_get_permissions(desc->ctx, desc->address, &perm);
        
        uint8_t target_mode = 0;
        if ((flags & VFS_OPEN_READ) && (perm & FS_PERMISSION_READ))    target_mode |= FS_FILE_MODE_READ;
        if ((flags & VFS_OPEN_WRITE) && (perm & FS_PERMISSION_WRITE))  target_mode |= FS_FILE_MODE_WRITE;
        if (target_mode == 0) {
            target_mode = FS_FILE_MODE_READ;
        }
        
        if (!fs_file_open(&desc->handle, desc->ctx, desc->address, target_mode)) {
            free(desc);
            return VFS_INVALID_FILE;
        }
    }
    
    if (!list_append(&open_files_head, &open_files_tail, desc)) {
        if (desc->in_file_system) {
            fs_file_close(&desc->handle);
        }
        free(desc);
        return VFS_INVALID_FILE;
    }
    
    return desc->id;
}

static void do_close(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return;
    if (desc->in_file_system) {
        fs_file_close(&desc->handle);
    }
    
    list_remove(&open_files_head, &open_files_tail, desc);
    free(desc);
}

static int32_t do_read(File file, void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return -1;
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->ctx, desc->address, &parent_perm);
        if (!(parent_perm & FS_PERMISSION_READ)) 
            return -1;
        return fs_file_read(&desc->handle, buffer, size);
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_READ)) 
            return -1;
        uint32_t node_size = kmalloc_get_size(desc->address);
        if (desc->offset >= node_size) return 0;
        if (desc->offset + size > node_size) {
            size = node_size - desc->offset;
        }
        
        kmem_read(buffer, desc->address + desc->offset, size);
        desc->offset += size;
        return (int32_t)size;
    }
}

static int32_t do_write(File file, const void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return -1;
    
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->ctx, desc->address, &parent_perm);
        if (!(parent_perm & FS_PERMISSION_WRITE)) 
            return -1;
        return fs_file_write(&desc->handle, buffer, size);
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return -1;
        uint32_t node_size = kmalloc_get_size(desc->address);
        if (desc->offset >= node_size) return -1;
        if (desc->offset + size > node_size) {
            size = node_size - desc->offset;
        }
        
        kmem_write(desc->address + desc->offset, buffer, size);
        desc->offset += size;
        return (int32_t)size;
    }
}

static uint32_t do_seek(File file, uint32_t position) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return VFS_INVALID_FILE;
    
    if (desc->in_file_system) {
        fs_file_seek(&desc->handle, position);
    }
    
    desc->offset = position;
    return position;
}

static uint32_t do_tell(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return VFS_INVALID_FILE;
    
    return (uint32_t)desc->offset;
}

static bool do_exists(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx) {
        if (!fs_file_check(ctx, address)) 
            if (!fs_check_directory_valid(ctx, address)) 
                return false;
        return true;
    }
    return kmalloc_is_valid(address);
}

static bool do_mkdir(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    char parent_path[256];
    char target_name[16];
    
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) return false;
    
    if (last_slash == path) {
        strcpy(parent_path, "/");
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) return false;
        
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    }
    target_name[sizeof(target_name) - 1] = '\0';
    
    // Reject (rather than silently truncate) names that are too long
    if (!vfs_name_is_valid(last_slash + 1))
        return false;
    
    uint32_t parent = resolve_path_to_address(parent_path);
    if (parent == 0xFFFFFFFF || parent == 0) {
        return false;
    }
    
    struct FSDeviceContext* ctx = vfs_device_get_context(parent_path);
    if (ctx && fs_check_directory_valid(ctx, parent)) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(ctx, parent, &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_READ) || 
            !(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        fs_directory_create(ctx, target_name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, parent);
        return true;
    } 
    else if (knode_check_is_valid(parent)) {
        uint8_t parent_perm = 0;
        knode_get_permissions(parent, &parent_perm);
        
        if (!(parent_perm & KMALLOC_PERMISSION_READ) || 
            !(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return false;
        create_knode(target_name, parent);
        return true;
    }
    return false;
}

static bool do_remove(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    uint32_t parent = resolve_parent_path_to_address(path);
    if (parent == 0xFFFFFFFF || parent == 0) 
        return false;
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, parent)) {
        uint8_t item_perm = 0;
        uint8_t parent_perm = 0;
        fs_file_get_permissions(ctx, address, &item_perm);
        fs_file_get_permissions(ctx, parent, &parent_perm);
        
        if (!(item_perm & FS_PERMISSION_WRITE) || 
            !(parent_perm & FS_PERMISSION_READ) || 
            !(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        if (fs_file_check(ctx, address)) {
            fs_directory_remove_reference(ctx, parent, address);
            return fs_file_delete(ctx, address);
        }
        
        // Directories go with everything inside them; basefs unlinks the
        // directory from its parent before freeing anything
        if (fs_check_directory_valid(ctx, address))
            return fs_directory_delete_recursive(ctx, address, parent);
        
        return false;
    } else {
        uint8_t item_perm = 0;
        uint8_t parent_perm = 0;
        knode_get_permissions(address, &item_perm);
        knode_get_permissions(parent, &parent_perm);
        
        if (!(item_perm & KMALLOC_PERMISSION_WRITE) || 
            !(parent_perm & KMALLOC_PERMISSION_READ) || 
            !(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return false;
        if (destroy_knode(address, parent)) 
            return true;
    }
    
    return false;
}

static bool do_rename(const char* path, const char* name) {
    if (path == NULL || path[0] == '\0' || path[0] == ' ') 
        return false;
    if (!vfs_name_is_valid(name)) 
        return false;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    
    uint32_t parent_address = resolve_parent_path_to_address(path);
    if (parent_address == 0xFFFFFFFF || parent_address == 0) {
        return false;
    }
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && fs_check_directory_valid(ctx, parent_address)) {
        // The lock makes this check-then-rename atomic: no other thread
        // can create a file with the same name in between.
        if (fs_directory_find(ctx, parent_address, name) != FS_NULL) {
            return false;
        }
        return fs_file_set_name(ctx, address, name);
    } else {
        knode_set_name(address, name);
        return true;
    }
}

static bool do_truncate(const char* path, uint32_t new_size) {
    if (path == NULL) 
        return false;
    if (path[0] == '\0' || path[0] == ' ') 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx) {
        fs_file_resize(ctx, address, new_size);
        return true;
    }
    return false;
}

static bool do_stat(const char* path, FSFileStats* stats) {
    if (path == NULL || path[0] == '\0' || stats == NULL) 
        return false;
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    memset(stats, 0, sizeof(FSFileStats));
    
    struct FSDeviceContext* ctx = vfs_device_get_context(path);
    if (ctx && (fs_file_check(ctx, address) || fs_check_directory_valid(ctx, address))) {
        uint8_t fs_perm = 0;
        fs_file_get_permissions(ctx, address, &fs_perm);
        
        stats->permissions = 0;
        if (fs_perm & FS_PERMISSION_READ)  stats->permissions |= VFS_PERMISSION_READ;
        if (fs_perm & FS_PERMISSION_WRITE) stats->permissions |= VFS_PERMISSION_WRITE;
        stats->size = fs_file_get_size(ctx, address);
        
        fs_file_get_certificate(ctx, address, &stats->certificate);
        return true;
    } 
    else if (knode_check_is_valid(address)) {
        uint8_t k_perm = kmalloc_get_permissions(address);
        stats->permissions = 0;
        if (k_perm & KMALLOC_PERMISSION_READ)  stats->permissions |= VFS_PERMISSION_READ;
        if (k_perm & KMALLOC_PERMISSION_WRITE) stats->permissions |= VFS_PERMISSION_WRITE;
        stats->size = kmalloc_get_size(address);
        stats->certificate = 0;
        return true;
    }
    
    return false;
}

static uint32_t do_get_size(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return 0;
    if (desc->in_file_system) {
        return fs_file_get_size(desc->ctx, desc->address);
    } else {
        return kmalloc_get_size(desc->address);
    }
}

//
// Public API: take the lock, run the operation, release
//

File vfs_open(const char* path, uint16_t flags) {
    vfs_lock();
    File result = do_open(path, flags);
    vfs_unlock();
    return result;
}

void vfs_close(File file) {
    vfs_lock();
    do_close(file);
    vfs_unlock();
}

int32_t vfs_read(File file, void* buffer, uint32_t size) {
    vfs_lock();
    int32_t result = do_read(file, buffer, size);
    vfs_unlock();
    return result;
}

int32_t vfs_write(File file, const void* buffer, uint32_t size) {
    vfs_lock();
    int32_t result = do_write(file, buffer, size);
    vfs_unlock();
    return result;
}

uint32_t vfs_seek(File file, uint32_t position) {
    vfs_lock();
    uint32_t result = do_seek(file, position);
    vfs_unlock();
    return result;
}

uint32_t vfs_tell(File file) {
    vfs_lock();
    uint32_t result = do_tell(file);
    vfs_unlock();
    return result;
}

bool vfs_exists(const char* path) {
    vfs_lock();
    bool result = do_exists(path);
    vfs_unlock();
    return result;
}

bool vfs_mkdir(const char* path) {
    vfs_lock();
    bool result = do_mkdir(path);
    vfs_unlock();
    return result;
}

bool vfs_remove(const char* path) {
    vfs_lock();
    bool result = do_remove(path);
    vfs_unlock();
    return result;
}

bool vfs_rename(const char* path, const char* name) {
    vfs_lock();
    bool result = do_rename(path, name);
    vfs_unlock();
    return result;
}

bool vfs_truncate(const char* path, uint32_t new_size) {
    vfs_lock();
    bool result = do_truncate(path, new_size);
    vfs_unlock();
    return result;
}

bool vfs_stat(const char* path, FSFileStats* stats) {
    vfs_lock();
    bool result = do_stat(path, stats);
    vfs_unlock();
    return result;
}

uint32_t vfs_get_size(File file) {
    vfs_lock();
    uint32_t result = do_get_size(file);
    vfs_unlock();
    return result;
}

