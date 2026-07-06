#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

File vfs_open(const char* path, uint16_t flags) {
    if (path == NULL || path[0] == '\0') 
        return VFS_INVALID_FILE;
    
    uint32_t current_knode = 0;
    uint32_t current_fs_node = 0;
    bool in_file_system = false;
    
    // Use our new parsing auxiliary function
    if (!vfs_parse_path(path, flags, &current_knode, &current_fs_node, &in_file_system)) {
        return VFS_INVALID_FILE;
    }
    
    // Directory check
    if (in_file_system) {
        if (!fs_file_check(current_fs_node)) {
            return VFS_INVALID_FILE;
        }
    } else {
        uint8_t k_flags = kmalloc_get_flags(current_knode);
        if (k_flags & KMALLOC_FLAG_DIRECTORY) {
            return VFS_INVALID_FILE;
        }
    }
    
    // Allocate our unique File Descriptor structure
    OpenFileDescriptor* desc = (OpenFileDescriptor*)malloc(sizeof(OpenFileDescriptor));
    if (!desc) 
        return VFS_INVALID_FILE;
    
    // Fill details based on where path resolution ended
    desc->id = next_unique_id++;
    desc->in_file_system = in_file_system;
    desc->address = in_file_system ? current_fs_node : current_knode;
    desc->offset = 0;
    desc->flags = flags;
    
    // If it's a file system node, bind and open the concrete backend handler
    if (desc->in_file_system) {
        uint8_t perm = 0;
        fs_file_get_permissions(desc->address, &perm);
        
        uint8_t target_mode = 0;
        if ((flags & VFS_OPEN_READ) && (perm & FS_PERMISSION_READ))    target_mode |= FS_FILE_MODE_READ;
        if ((flags & VFS_OPEN_WRITE) && (perm & FS_PERMISSION_WRITE))  target_mode |= FS_FILE_MODE_WRITE;
        
        // Fallback baseline mode if zero options specified
        if (target_mode == 0) {
            target_mode = FS_FILE_MODE_READ;
        }
        
        if (!fs_file_open(&desc->handle, desc->address, target_mode)) {
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

void vfs_close(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return;
    
    if (desc->in_file_system) {
        fs_file_close(&desc->handle);
    }
    
    list_remove(&open_files_head, &open_files_tail, desc);
    free(desc);
}

int32_t vfs_read(File file, void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return -1;
    
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->address, &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_READ)) 
            return -1;
        
        return fs_file_read(&desc->handle, buffer, size);
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_READ)) 
            return -1;
            
        // Bounds check for memory-backed nodes if necessary
        uint32_t node_size = kmalloc_get_size(desc->address);
        if (desc->offset >= node_size) return 0; // EOF
        if (desc->offset + size > node_size) {
            size = node_size - desc->offset;
        }
        
        kmem_read(buffer, desc->address + desc->offset, size);
        desc->offset += size;
        return (int32_t)size;
    }
}

int32_t vfs_write(File file, const void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return -1;
    
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->address, &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_WRITE)) 
            return -1;
        
        return fs_file_write(&desc->handle, buffer, size);
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return -1;
        
        uint32_t node_size = kmalloc_get_size(desc->address);
        if (desc->offset >= node_size) return -1; // Out of fixed space
        if (desc->offset + size > node_size) {
            size = node_size - desc->offset;
        }
        
        kmem_write(desc->address + desc->offset, buffer, size);
        desc->offset += size;
        return (int32_t)size;
    }
}

uint32_t vfs_seek(File file, uint32_t position) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return VFS_INVALID_FILE;
    
    if (desc->in_file_system) {
        fs_file_seek(&desc->handle, position);
    }
    
    desc->offset = position;
    return position;
}

uint32_t vfs_tell(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return VFS_INVALID_FILE;
    
    return (uint32_t)desc->offset;
}

bool vfs_exists(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    if (!fs_file_check(address)) 
        if (!fs_check_directory_valid(address)) 
            return false;
    return true;
}

bool vfs_mkdir(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    char parent_path[256];
    char target_name[16];
    
    // Find the last occurrence of '/' to isolate the directory name
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
    
    // Resolve parent directory address and check if it exists
    uint32_t parent = resolve_path_to_address(parent_path);
    if (parent == 0xFFFFFFFF || parent == 0) {
        return false; // Leading path does not exist
    }
    
    // Check file system directory
    if (fs_check_directory_valid(parent)) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(parent,  &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_READ) || 
            !(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        
        fs_directory_create(target_name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, parent);
        return true;
    } 
    
    // Knode directory
    else if (knode_check_is_valid(parent)) {
        uint8_t parent_perm = 0;
        knode_get_permissions(parent,  &parent_perm);
        
        if (!(parent_perm & KMALLOC_PERMISSION_READ) || 
            !(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return false;
        
        create_knode(target_name, parent);
        return true;
    }
    return false;
}

bool vfs_remove(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    
    uint32_t parent = resolve_parent_path_to_address(path);
    if (parent == 0xFFFFFFFF || parent == 0) 
        return false;
    
    if (fs_check_directory_valid(parent)) {
        uint8_t item_perm = 0;
        uint8_t parent_perm = 0;
        fs_file_get_permissions(address, &item_perm);
        fs_file_get_permissions(parent,  &parent_perm);
        
        if (!(item_perm & FS_PERMISSION_WRITE) || 
            !(parent_perm & FS_PERMISSION_READ) || 
            !(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        
        fs_directory_remove_reference(parent, address);
        
        if (!fs_file_delete(address)) {
            if (fs_directory_delete(address)) 
                return true;
        } else {
            return true;
        }
        
    } else {
        uint8_t item_perm = 0;
        uint8_t parent_perm = 0;
        knode_get_permissions(address, &item_perm);
        knode_get_permissions(parent,  &parent_perm);
        
        if (!(item_perm & KMALLOC_PERMISSION_WRITE) || 
            !(parent_perm & KMALLOC_PERMISSION_READ) || 
            !(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return false;
        
        if (destroy_knode(address, parent)) 
            return true;
    }
    
    return false;
}

bool vfs_rename(const char* path, const char* name) {
    if (path == NULL || name == NULL) 
        return false;
    
    if (path[0] == '\0' || name[0] == '\0' || 
        path[0] == ' ' || name[0] == ' ') 
        return false;
    
    // Resolve the target file/directory's current internal address
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false; // Target item does not exist
    
    // Resolve the parent directory's address to determine the context
    uint32_t parent_address = resolve_parent_path_to_address(path);
    if (parent_address == 0xFFFFFFFF || parent_address == 0) {
        return false; // Unable to locate parent directory context
    }
    // Check if the name is taken
    if (fs_directory_find(parent_address, name) != FS_NULL) {
        return false;
    }
    
    // Perform the rename operation based on the architecture layer
    if (fs_check_directory_valid(parent_address)) {
        fs_file_set_name(address, name);
        return true;
    } else {
        knode_set_name(address, name);
        return true;
    }
}

bool vfs_truncate(const char* path, uint32_t new_size) {
    if (path == NULL) 
        return false;
    
    if (path[0] == '\0' || path[0] == ' ') 
        return false;
    
    // Resolve the target file/directory's current internal address
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    
    fs_file_resize(address, new_size);
    return true;
}

bool vfs_stat(const char* path, FSFileStats* stats) {
    if (path == NULL || path[0] == '\0' || stats == NULL) 
        return false;
    
    // Resolve the path to its target address
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) 
        return false;
    
    memset(stats, 0, sizeof(FSFileStats));
    
    if (fs_file_check(address) || fs_check_directory_valid(address)) {
        uint8_t fs_perm = 0;
        fs_file_get_permissions(address, &fs_perm);
        
        stats->permissions = 0;
        if (fs_perm & FS_PERMISSION_READ)  stats->permissions |= VFS_PERMISSION_READ;
        if (fs_perm & FS_PERMISSION_WRITE) stats->permissions |= VFS_PERMISSION_WRITE;
        
        // Fetch size
        stats->size = fs_file_get_size(address);
        
        fs_file_get_certificate(address, &stats->certificate);
        stats->certificate = 0;
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

uint32_t vfs_get_size(File file) {
    OpenFileDescriptor* desc = vfs_file_find_open(file);
    if (!desc) return false;
    // Check currently in a mounted file system
    if (desc->in_file_system) {
        return fs_file_get_size(desc->address);
    } else {
        return kmalloc_get_size(desc->address);
    }
    
}
