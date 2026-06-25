#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/malloc.h>

#include <kernel/vfs/vfs.h>

#include <kernel/util/string.h>
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
    
    char* token = strtok(path_scratch, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
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
                    if (fs_device_open(device_address, &partition) == 0) {
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
                fs_device_open(device_address, &partition);
                
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
                    char* next_token = strtok(NULL, "/");
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
        token = strtok(NULL, "/");
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
        uint8_t target_mode = 0;
        if (flags & VFS_OPEN_READ)   target_mode |= FS_FILE_MODE_READ;
        if (flags & VFS_OPEN_WRITE)  target_mode |= FS_FILE_MODE_WRITE;
        
        // Fallback baseline mode if zero options specified
        if (target_mode == 0) {
            target_mode = FS_FILE_MODE_READ;
        }
        
        if (!fs_file_open(&desc->handle, desc->address, target_mode)) {
            free(desc);
            return INVALID_FILE_ID;
        }
    }
    
    // Register with the link list tracker
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

void vfs_read(File file, void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return;
    
    if (desc->in_file_system) {
        // Leverages your filesystem API to process allocation metadata boundaries safely
        fs_file_read(&desc->handle, buffer, size);
    } else {
        kmem_read(buffer, desc->address + desc->offset, size);
        desc->offset += size;
    }
}

void vfs_write(File file, const void* buffer, uint32_t size) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return;
    
    if (desc->in_file_system) {
        // Directly writes payload blocks through your verified sub-system
        fs_file_write(&desc->handle, buffer, size);
    } else {
        kmem_write(desc->address + desc->offset, buffer, size);
        desc->offset += size;
    }
}

void vfs_mkfile(const char* path, uint32_t size) {
    if (path == NULL || path[0] == '\0') return;
    
    char parent_path[256];
    char target_name[16];
    
    // Find the last occurrence of '/' to isolate the filename
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) return; // Expecting absolute paths
    
    if (last_slash == path) {
        // The parent directory is the root "/"
        strcpy(parent_path, "/");
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) return; // Path overflow safety
        
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    }
    target_name[sizeof(target_name) - 1] = '\0';
    
    // Resolve parent directory address and check if it exists
    uint32_t parent_address = resolve_path_to_address(parent_path);
    if (parent_address == 0xFFFFFFFF || parent_address == 0) {
        return; // Leading path does not exist
    }
    
    // Ensure creations happen inside an active filesystem context
    if (fs_check_directory_valid(parent_address)) {
        fs_file_create(target_name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, size, parent_address);
    }
}

void vfs_mkdir(const char* path) {
    if (path == NULL || path[0] == '\0') return;
    
    char parent_path[256];
    char target_name[16];
    
    // Find the last occurrence of '/' to isolate the directory name
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) return;
    
    if (last_slash == path) {
        strcpy(parent_path, "/");
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) return;
        
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strncpy(target_name, last_slash + 1, sizeof(target_name) - 1);
    }
    target_name[sizeof(target_name) - 1] = '\0';
    
    // Resolve parent directory address and check if it exists
    uint32_t parent_address = resolve_path_to_address(parent_path);
    if (parent_address == 0xFFFFFFFF || parent_address == 0) {
        return; // Leading path does not exist
    }
    
    if (fs_check_directory_valid(parent_address)) {
        fs_directory_create(target_name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, parent_address);
    }
}

void vfs_remove(const char* path) {
    if (path == NULL || path[0] == '\0') return;
    
    // Resolve the direct target element address
    uint32_t address = resolve_path_to_address(path);
    if (address == 0xFFFFFFFF || address == 0) {
        return; // Target element or its leading path does not exist
    }
    
    // Route to appropriate concrete driver removal logic
    if (fs_check_directory_valid(address)) {
        fs_directory_delete(address);
    } else if (fs_file_check(address)) {
        fs_file_delete(address);
    }
}

bool vfs_set_permissions(File file, uint8_t perm) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return false;
    
    uint8_t permissions = 0;
    if (desc->in_file_system) {
        if (perm & VFS_PERMISSION_EXECUTE)  permissions |= FS_PERMISSION_EXECUTE;
        if (perm & VFS_PERMISSION_READ)     permissions |= FS_PERMISSION_READ;
        if (perm & VFS_PERMISSION_WRITE)    permissions |= FS_PERMISSION_WRITE;
        
        fs_file_set_permissions(desc->address, permissions);
    } else {
        if (perm & VFS_PERMISSION_EXECUTE)  permissions |= KMALLOC_PERMISSION_EXECUTABLE;
        if (perm & VFS_PERMISSION_READ)     permissions |= KMALLOC_PERMISSION_READ;
        if (perm & VFS_PERMISSION_WRITE)    permissions |= KMALLOC_PERMISSION_WRITE;
        
        kmalloc_set_permissions(desc->address, permissions); 
    }
    
    return true; 
}

bool vfs_get_permissions(File file, uint8_t* perm) {
    OpenFileDescriptor* desc = vfs_find_descriptor_by_id(file);
    if (!desc) return false;
    
    *perm = 0;
    uint8_t permissions=0;
    if (desc->in_file_system) {
        fs_file_get_permissions(desc->address, &permissions);
        
        *perm = 0;
        if (permissions & FS_PERMISSION_EXECUTE) *perm |= VFS_PERMISSION_EXECUTE;
        if (permissions & FS_PERMISSION_READ)    *perm |= VFS_PERMISSION_READ;
        if (permissions & FS_PERMISSION_WRITE)   *perm |= VFS_PERMISSION_WRITE;
        
    } else {
        permissions = kmalloc_get_permissions(desc->address);
        
        if (permissions & KMALLOC_PERMISSION_EXECUTABLE) *perm |= VFS_PERMISSION_EXECUTE;
        if (permissions & KMALLOC_PERMISSION_READ)       *perm |= VFS_PERMISSION_READ;
        if (permissions & KMALLOC_PERMISSION_WRITE)      *perm |= VFS_PERMISSION_WRITE;
        
    }
    
    return true;
}

uint32_t vfs_get_size(File file) {
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

uint32_t resolve_path_to_address(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return 0xFFFFFFFF;
    
    // Initialize starting location
    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0; // 0 means we are still in knode space
    bool in_file_system = false;
    
    // Create a local copy of the path for destructive tokenization (strtok)
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    
    // Handle explicit starting points if necessary 
    // (Assuming standard absolute path starts with '/')
    char* token = strtok(path_scratch, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Skip current directory references
            token = strtok(NULL, "/");
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
                    if (fs_device_open(device_address, &partition) == 0) {
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
                fs_device_open(device_address, &partition);
                
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
                
                    char item_name[16]; // Match MAX_TITLE_LEN
                    fs_file_get_name(reference, item_name);
                
                    if (strcmp(item_name, token) == 0) {
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
        
        token = strtok(NULL, "/");
    }
    
    // Return the correct address context based on where traversal ended
    return in_file_system ? current_fs_node : current_knode;
}
