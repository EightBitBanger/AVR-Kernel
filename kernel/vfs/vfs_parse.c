#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/malloc.h>

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/vfs/vfs_internal.h>

#include <kernel/util/string.h>
#include <kernel/util/tok.h>
#include <kernel/util/list.h>

uint32_t resolve_path_to_address(const char* path) {
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

uint32_t resolve_parent_path_to_address(const char* path) {
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

bool vfs_parse_path(const char* path, uint16_t flags, uint32_t* out_knode, uint32_t* out_fs_node, bool* out_in_fs) {
    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0; 
    bool in_file_system = false;
    
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    
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
        
        strncpy(last_token, token, sizeof(last_token) - 1);
        last_token[sizeof(last_token) - 1] = '\0';
        
        if (!in_file_system) {
            if (strcmp(token, "..") == 0) {
                current_knode = knode_get_parent(current_knode);
            } else {
                uint32_t next_node = knode_find_by_name(current_knode, token);
                if (next_node == KNODE_NULL || next_node == 0) {
                    return false; 
                }
                current_knode = next_node;
            }
            
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
                    char* next_token = cstr_tok_next(&tok);
                    if (next_token == NULL && in_file_system && (flags & VFS_OPEN_CREATE)) {
                        uint8_t default_perms = FS_PERMISSION_READ | FS_PERMISSION_WRITE;
                        uint32_t new_file_address = fs_file_create(last_token, default_perms, 0, parent_fs_node);
                        
                        if (new_file_address == FS_NULL) {
                            return false;
                        }
                        current_fs_node = new_file_address;
                        break; 
                    }
                    return false; 
                }
                current_fs_node = found_ref;
            }
        }
        token = cstr_tok_next(&tok);
    }
    
    *out_knode = current_knode;
    *out_fs_node = current_fs_node;
    *out_in_fs = in_file_system;
    return true;
}
