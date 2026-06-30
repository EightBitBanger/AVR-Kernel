#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

typedef struct {
    File id;              // Unique integer ID returned to the user
    uint32_t address;     // Resolved knode address or fs_node address
    bool in_file_system;  // Distinguishes between knode and block file system
    uint32_t offset;      // Positions tracker for virtual knodes
    FileHandle handle;    // Embedded File system handle for physical file system IO
    uint16_t flags;       // Flags relating to the state of the opened file
} OpenFileDescriptor;

static struct list_node* open_files_head = NULL;
static struct list_node* open_files_tail = NULL;
static File next_unique_id = 1;

static uint32_t resolve_path_to_address(const char* path);
static uint32_t resolve_parent_path_to_address(const char* path);

static OpenFileDescriptor* vfs_find_descriptor_by_id(File id) {
    if (id == INVALID_FILE_ID) return NULL;
    
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

File vfs_open(const char* path, uint16_t flags) {
    if (path == NULL || path[0] == '\0') {
        return INVALID_FILE_ID;
    }
    
    // Trace the path down to resolve the address and system context
    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0; 
    bool in_file_system = false;
    
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    
    // Track parent directory context and the filename token
    uint32_t parent_fs_node = 0;
    char last_token[16] = {0};
    
    cstr_tok_t tok;
    cstr_tok_init(&tok, path_scratch, "/");
    
    char* token = cstr_tok_next(&tok);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = cstr_tok_next(&tok);
            continue;
        }
        
        // Keep track of the current segment name
        strncpy(last_token, token, sizeof(last_token) - 1);
        last_token[sizeof(last_token) - 1] = '\0';
        
        if (!in_file_system) {
            if (strcmp(token, "..") == 0) {
                current_knode = knode_get_parent(current_knode);
            } else {
                uint32_t next_node = knode_find_by_name(current_knode, token);
                if (next_node == KNODE_NULL || next_node == 0) {
                    return INVALID_FILE_ID; 
                }
                current_knode = next_node;
            }
            
            // Detect mount point boundaries
            uint8_t k_flags = kmalloc_get_flags(current_knode);
            if (k_flags & KMALLOC_FLAG_MOUNT) {
                uint32_t device_address = knode_get_reference(current_knode, 0);
                if (device_address != KNODE_NULL && device_address != 0) {
                    struct FSPartitionBlock partition;
                    if (fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA) == 0) {
                        current_fs_node = partition.root_directory;
                        in_file_system = true;
                    }
                }
            }
        } else {
            if (strcmp(token, "..") == 0) {
                uint32_t parent = fs_directory_get_parent(current_fs_node);
                struct FSPartitionBlock partition;
                uint32_t device_address = knode_get_reference(current_knode, 0);
                fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA);
                
                if (current_fs_node == partition.root_directory) {
                    in_file_system = false;
                    current_fs_node = 0;
                    current_knode = knode_get_parent(current_knode);
                } else {
                    current_fs_node = parent;
                }
            } else {
                uint32_t ref_count = fs_directory_get_reference_count(current_fs_node);
                uint32_t found_ref = 0;
                
                for (uint32_t i = 0; i < ref_count; i++) {
                    uint32_t reference = fs_directory_get_reference(current_fs_node, i);
                    if (reference == FS_NULL) continue;
                    
                    char item_name[16]; 
                    fs_file_get_name(reference, item_name);
                    
                    if (strcmp(item_name, token) == 0) {
                        found_ref = reference;
                        break;
                    }
                }
                
                parent_fs_node = current_fs_node;
                
                if (found_ref == 0 || found_ref == FS_NULL) {
                    // Peek ahead to check if this is the final missing component
                    char* next_token = cstr_tok_next(&tok);
                    if (next_token == NULL && in_file_system && (flags & VFS_OPEN_CREATE)) {
                        // Handle file creation within physical file system
                        uint8_t default_perms = FS_PERMISSION_READ | FS_PERMISSION_WRITE;
                        uint32_t new_file_address = fs_file_create(last_token, default_perms, 0, parent_fs_node);
                        
                        if (new_file_address == FS_NULL) {
                            return INVALID_FILE_ID;
                        }
                        current_fs_node = new_file_address;
                        break; 
                    }
                    
                    return INVALID_FILE_ID; 
                }
                current_fs_node = found_ref;
            }
        }
        token = cstr_tok_next(&tok);
    }
    
    // Directory check
    if (in_file_system) {
        // fs_file_check returns false if target node has directory attributes
        if (!fs_file_check(current_fs_node)) {
            return INVALID_FILE_ID;
        }
    } else {
        // Query the heap allocator tags for the raw knode
        uint8_t k_flags = kmalloc_get_flags(current_knode);
        if (k_flags & KMALLOC_FLAG_DIRECTORY) {
            return INVALID_FILE_ID;
        }
    }
    
    // Allocate our unique File Descriptor structure
    OpenFileDescriptor* desc = (OpenFileDescriptor*)malloc(sizeof(OpenFileDescriptor));
    if (!desc) {
        return INVALID_FILE_ID;
    }
    
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
            return INVALID_FILE_ID;
        }
    }
    
    // Register with the linked list tracker
    if (!list_append(&open_files_head, &open_files_tail, desc)) {
        if (desc->in_file_system) {
            fs_file_close(&desc->handle);
        }
        free(desc);
        return INVALID_FILE_ID;
    }
    
    return desc->id;
}

void vfs_close(File file) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return;
    
    // Explicitly unregister and terminate the underlying FS descriptor mapping
    if (desc->in_file_system) {
        fs_file_close(&desc->handle);
    }
    
    list_remove(&open_files_head, &open_files_tail, desc);
    free(desc);
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

bool vfs_file_read(File file, void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return false;
    
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->address, &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_READ)) 
            return false;
        
        fs_file_read(&desc->handle, buffer, size);
        return true;
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_READ)) 
            return false;
        
        kmem_read(buffer, desc->address + desc->offset, size);
        desc->offset += size;
        return true;
    }
}

bool vfs_file_write(File file, const void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return false;
    
    if (desc->in_file_system) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(desc->address, &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        
        fs_file_write(&desc->handle, buffer, size);
        return true;
    } else {
        uint8_t parent_perm = 0;
        parent_perm = kmalloc_get_permissions(desc->address);
        
        if (!(parent_perm & KMALLOC_PERMISSION_WRITE)) 
            return false;
        
        kmem_write(desc->address + desc->offset, buffer, size);
        desc->offset += size;
        return true;
    }
}

bool vfs_mkfile(const char* path, uint32_t size) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    char parent_path[256];
    char target_name[16];
    
    // Find the last occurrence of '/' to isolate the filename
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) return false; // Expecting absolute paths
    
    if (last_slash == path) {
        // The parent directory is the root "/"
        strcpy(parent_path, "/");
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) return false; // Path overflow safety
        
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
    
    // Ensure creations happen inside an active filesystem context
    if (fs_check_directory_valid(parent)) {
        uint8_t parent_perm = 0;
        fs_file_get_permissions(parent,  &parent_perm);
        
        if (!(parent_perm & FS_PERMISSION_READ) || 
            !(parent_perm & FS_PERMISSION_WRITE)) 
            return false;
        
        fs_file_create(target_name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, size, parent);
        return true;
    }
    return false;
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
    if (address == 0xFFFFFFFF || address == 0) {
        return false; // Target item does not exist
    }
    
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
    if (vfs_remove(path)) 
        if (vfs_mkfile(path, new_size)) 
            return true;
    // TODO implement a proper 'fs_file_resize' function
    return false;
}

bool vfs_set_permissions(const char* path, uint8_t perm) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) {
        return false; // Path does not exist
    }
    
    // Determine context: underlying physical file system or raw knode
    if (fs_check_directory_valid(address) || fs_file_check(address)) {
        uint8_t permissions = 0;
        if (perm & VFS_PERMISSION_EXECUTE)  permissions |= FS_PERMISSION_EXECUTE;
        if (perm & VFS_PERMISSION_READ)     permissions |= FS_PERMISSION_READ;
        if (perm & VFS_PERMISSION_WRITE)    permissions |= FS_PERMISSION_WRITE;
        
        fs_file_set_permissions(address, permissions);
    } else if (kmalloc_is_valid(address)) {
        uint8_t permissions = 0;
        if (perm & VFS_PERMISSION_EXECUTE)  permissions |= KMALLOC_PERMISSION_EXECUTABLE;
        if (perm & VFS_PERMISSION_READ)     permissions |= KMALLOC_PERMISSION_READ;
        if (perm & VFS_PERMISSION_WRITE)    permissions |= KMALLOC_PERMISSION_WRITE;
        
        kmalloc_set_permissions(address, permissions); 
    } else {
        return false; // Unknown address space context
    }
    
    return true; 
}

bool vfs_get_permissions(const char* path, uint8_t* perm) {
    if (path == NULL || path[0] == '\0' || perm == NULL) {
        return false;
    }
    
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) {
        return false; // Path does not exist
    }
    
    *perm = 0;
    
    // Check if the address context is a filesystem block or memory knode
    if (fs_check_directory_valid(address) || fs_file_check(address)) {
        uint8_t permissions = 0;
        fs_file_get_permissions(address, &permissions);
        
        if (permissions & FS_PERMISSION_EXECUTE) *perm |= VFS_PERMISSION_EXECUTE;
        if (permissions & FS_PERMISSION_READ)    *perm |= VFS_PERMISSION_READ;
        if (permissions & FS_PERMISSION_WRITE)   *perm |= VFS_PERMISSION_WRITE;
        
    } else if (kmalloc_is_valid(address)) {
        uint8_t permissions = 0;
        permissions = kmalloc_get_permissions(address);
        
        if (permissions & KMALLOC_PERMISSION_EXECUTABLE) *perm |= VFS_PERMISSION_EXECUTE;
        if (permissions & KMALLOC_PERMISSION_READ)       *perm |= VFS_PERMISSION_READ;
        if (permissions & KMALLOC_PERMISSION_WRITE)      *perm |= VFS_PERMISSION_WRITE;
        
    } else {
        return false;
    }

    return true;
}

uint32_t vfs_file_get_size(File file) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return false;
    // Check currently in a mounted file system
    if (desc->in_file_system) {
        return fs_file_get_size(desc->address);
    } else {
        return kmalloc_get_size(desc->address);
    }
    
}

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

bool vfs_is_directory_mounted(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return false;
    
    uint32_t address = resolve_path_to_address(path);
    if (address == KNODE_NULL || address == FS_NULL) 
        return false;
    
    // The path pointed to a raw knode mount point that wasn't fully entered
    if (kmalloc_is_valid(address)) {
        uint8_t k_flags = kmalloc_get_flags(address);
        if (k_flags & KMALLOC_FLAG_MOUNT) {
            return true;
        }
    }
    
    // The path pointed to a resolved filesystem node.
    // Check if this address matches the root directory of its device partition.
    if (fs_check_directory_valid(address)) {
        // If the filesystem directory's parent goes above the root, 
        // or if it matches its partition block root directory:
        uint32_t parent = fs_directory_get_parent(address);
        if (parent == address || parent == FS_NULL) { 
            return true;
        }
    }
    
    return false;
}

uint32_t vfs_directory_count(const char* path) {
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

static uint32_t resolve_path_to_address(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return 0xFFFFFFFF;
    
    // Initialize starting location
    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0; // 0 means we are still in knode space
    bool in_file_system = false;
    
    // Create a local copy of the path for tokenization
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    
    // Handle explicit starting points if necessary 
    // (Assuming standard absolute path starts with '/')
    cstr_tok_t tok;
    cstr_tok_init(&tok, path_scratch, "/");
    
    char* token = cstr_tok_next(&tok);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Skip current directory references
            token = cstr_tok_next(&tok);
            continue;
        }
        
        if (!in_file_system) {
            // Traversing knode directories
            if (strcmp(token, "..") == 0) {
                current_knode = knode_get_parent(current_knode);
            } else {
                uint32_t next_node = knode_find_by_name(current_knode, token);
                if (next_node == 0xFFFFFFFF || next_node == 0) {
                    return 0xFFFFFFFF; // Path segment not found
                }
                current_knode = next_node;
            }
            
            // Check if this new knode is actually a mount point into a file system
            uint8_t flags = kmalloc_get_flags(current_knode);
            if (flags & KMALLOC_FLAG_MOUNT) {
                // Fetch the device address attached to the mount point (reference index 0)
                uint32_t device_address = knode_get_reference(current_knode, 0);
                if (device_address != KMALLOC_NULL && device_address != 0) {
                    struct FSPartitionBlock partition;
                    if (fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA) == 0) {
                        // Switch over to File System mode starting at the partition's root
                        current_fs_node = partition.root_directory;
                        in_file_system = true;
                    }
                }
            }
        } else {
            // Traversing a mounted file system
            if (strcmp(token, "..") == 0) {
                uint32_t parent = fs_directory_get_parent(current_fs_node);
                
                // If we attempt to go above the FS root, we fall back to the base knode
                struct FSPartitionBlock partition;
                uint32_t device_address = knode_get_reference(current_knode, 0);
                fs_device_open(device_address, &partition, FS_DEVICE_TYPE_ATA);
                
                if (current_fs_node == partition.root_directory) {
                    in_file_system = false;
                    current_fs_node = 0;
                    current_knode = knode_get_parent(current_knode);
                } else {
                    current_fs_node = parent;
                }
            } else {
                // Search the current file system directory for the child matching 'token'
                uint32_t ref_count = fs_directory_get_reference_count(current_fs_node);
                uint32_t found_ref = 0;
                
                for (uint32_t i = 0; i < ref_count; i++) {
                    uint32_t reference = fs_directory_get_reference(current_fs_node, i);
                    if (reference == FS_NULL) continue;
                    
                    char item_name[32];
                    fs_file_get_name(reference, item_name);
                    
                    if (strncmp(item_name, token, 32) == 0) {
                        found_ref = reference;
                        break;
                    }
                }
                
                if (found_ref == 0 || found_ref == FS_NULL) {
                    return 0xFFFFFFFF; // Path component not found in file system
                }
                current_fs_node = found_ref;
            }
        }
        
        token = cstr_tok_next(&tok);
    }
    
    // Return the correct address context based on where traversal ended
    return in_file_system ? current_fs_node : current_knode;
}

static uint32_t resolve_parent_path_to_address(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return 0xFFFFFFFF;
    }
    
    char parent_path[256];
    
    // Find the last occurrence of '/' to isolate the parent directory path
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        // If there's no slash, it's a relative path in the current directory.
        // Assuming relative paths fall back to current directory, or return error if absolute is required.
        strcpy(parent_path, "."); 
    }
    else if (last_slash == path) {
        // The parent directory is the root "/" (e.g., "/filename")
        strcpy(parent_path, "/");
    } 
    else {
        // Extract everything before the last slash (e.g., "/mnt/filename" -> "/mnt")
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) {
            return 0xFFFFFFFF; // Path overflow safety
        }
        
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }
    
    // Pass the isolated parent directory path to your existing path resolver
    return resolve_path_to_address(parent_path);
}
