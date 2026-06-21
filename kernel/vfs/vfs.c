#include <stdint.h>
#include <stdbool.h>
#include <kernel/arch/x86/malloc.h>

#include <kernel/vfs/vfs.h>

#include <kernel/util/string.h>
#include <kernel/util/list.h>

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
    
    // We need to keep track of the parent directory in case we need to create the file
    uint32_t parent_fs_node = 0;
    char last_token[16] = {0};
    
    char* token = strtok(path_scratch, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        
        // Save the current token as the potential filename if it's the last one
        strncpy(last_token, token, sizeof(last_token) - 1);
        
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
            uint8_t k_flags = kmalloc_get_flags(current_knode); // renamed from 'flags' to avoid shadowing parameter
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
                
                // Track parent context right before hopping into the next child node
                parent_fs_node = current_fs_node;
                
                if (found_ref == 0 || found_ref == FS_NULL) {
                    // Peek ahead: Is this the final missing file component?
                    char* next_token = strtok(NULL, "/");
                    if (next_token == NULL && in_file_system && (flags & VFS_OPEN_CREATE)) {
                        // Handle VFS_OPEN_CREATE: missing target file, instantiate it!
                        // Default size 0, default permissions matching standard user reads/writes
                        uint8_t default_perms = FS_PERMISSION_READ | FS_PERMISSION_WRITE;
                        uint32_t new_file_address = fs_file_create(last_token, default_perms, 0, parent_fs_node);
                        
                        if (new_file_address == FS_NULL) {
                            return INVALID_FILE_ID;
                        }
                        current_fs_node = new_file_address;
                        break; // Successfully broke out, path is now resolved
                    }
                    
                    return INVALID_FILE_ID; 
                }
                current_fs_node = found_ref;
            }
        }
        token = strtok(NULL, "/");
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
        // Map VFS flags directly down into your physical FS implementation's modes
        uint8_t target_mode = 0;
        if (flags & VFS_OPEN_READ)   target_mode |= FS_FILE_MODE_READ;
        if (flags & VFS_OPEN_WRITE)  target_mode |= FS_FILE_MODE_WRITE;
        if (flags & VFS_OPEN_APPEND) target_mode |= FS_FILE_MODE_APPEND;
        
        // Default catch-all if zero flags are specified 
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

uint32_t resolve_path_to_address(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return KNODE_NULL;
    }

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
            // --- TRAVERSING KNODE SPACE ---
            if (strcmp(token, "..") == 0) {
                current_knode = knode_get_parent(current_knode);
            } else {
                uint32_t next_node = knode_find_by_name(current_knode, token);
                if (next_node == KNODE_NULL || next_node == 0) {
                    return KNODE_NULL; // Path segment not found
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
            // --- TRAVERSING MOUNTED FILE SYSTEM SPACE ---
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
                    return KNODE_NULL; // Path component not found in file system
                }
                current_fs_node = found_ref;
            }
        }

        token = strtok(NULL, "/");
    }

    // Return the correct address context based on where traversal ended
    return in_file_system ? current_fs_node : current_knode;
}
