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

    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0;
    bool in_file_system = false;
    struct FSDeviceContext* current_ctx = NULL;
    
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    
    cstr_tok_t tok;
    cstr_tok_init(&tok, path_scratch, "/");
    
    char* token = cstr_tok_next(&tok);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = cstr_tok_next(&tok);
            continue;
        }
        
        if (!in_file_system) {
            if (strcmp(token, "..") == 0) {
                current_knode = knode_get_parent(current_knode);
            } else {
                uint32_t next_node = knode_find_by_name(current_knode, token);
                if (next_node == 0xFFFFFFFF || next_node == 0) {
                    return 0xFFFFFFFF;
                }
                current_knode = next_node;
            }
            
            uint8_t flags = kmalloc_get_flags(current_knode);
            if (flags & KMALLOC_FLAG_MOUNT) {
                uint32_t device_address = knode_get_reference(current_knode, 0);
                struct FSDeviceContext* device_context = (struct FSDeviceContext*)knode_get_reference(current_knode, 1);
                
                if ((uint32_t)device_context != KMALLOC_NULL && device_context != NULL) {
                    if (device_address != KMALLOC_NULL && device_address != 0) {
                        struct FSPartitionBlock partition;
                        fs_device_get_partition(device_context, &partition);
                        
                        if (device_context->is_open == true) {
                            current_fs_node = partition.root_directory;
                            current_ctx = device_context;
                            in_file_system = true;
                        }
                    }
                }
            }
        } else {
            if (strcmp(token, "..") == 0) {
                uint32_t parent = fs_directory_get_parent(current_ctx, current_fs_node);
                struct FSPartitionBlock partition;
                fs_device_get_partition(current_ctx, &partition);
                
                if (current_fs_node == partition.root_directory) {
                    in_file_system = false;
                    current_fs_node = 0;
                    current_ctx = NULL;
                    current_knode = knode_get_parent(current_knode);
                } else {
                    current_fs_node = parent;
                }
            } else {
                uint32_t ref_count = fs_directory_get_reference_count(current_ctx, current_fs_node);
                uint32_t found_ref = 0;
                
                for (uint32_t i = 0; i < ref_count; i++) {
                    uint32_t reference = fs_directory_get_reference(current_ctx, current_fs_node, i);
                    if (reference == FS_NULL) continue;
                    
                    char item_name[32];
                    fs_file_get_name(current_ctx, reference, item_name);
                    
                    if (strncmp(item_name, token, 32) == 0) {
                        found_ref = reference;
                        break;
                    }
                }
                
                if (found_ref == 0 || found_ref == FS_NULL) {
                    return 0xFFFFFFFF;
                }
                current_fs_node = found_ref;
            }
        }
        
        token = cstr_tok_next(&tok);
    }
    
    return in_file_system ? current_fs_node : current_knode;
}

uint32_t resolve_path_to_mount_point(const char* path) {
    if (path == NULL || path[0] == '\0') 
        return 0xFFFFFFFF;
    uint32_t current_knode = knode_get_root();
    
    if (kmalloc_get_flags(current_knode) & KMALLOC_FLAG_MOUNT) {
        return current_knode;
    }
    
    char path_scratch[256];
    strncpy(path_scratch, path, sizeof(path_scratch) - 1);
    path_scratch[sizeof(path_scratch) - 1] = '\0';
    cstr_tok_t tok;
    cstr_tok_init(&tok, path_scratch, "/");
    
    char* token = cstr_tok_next(&tok);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = cstr_tok_next(&tok);
            continue;
        }
        
        if (strcmp(token, "..") == 0) {
            current_knode = knode_get_parent(current_knode);
        } else {
            uint32_t next_node = knode_find_by_name(current_knode, token);
            if (next_node == 0xFFFFFFFF || next_node == 0) {
                return 0xFFFFFFFF;
            }
            current_knode = next_node;
        }
        
        uint8_t flags = kmalloc_get_flags(current_knode);
        if (flags & KMALLOC_FLAG_MOUNT) {
            return current_knode;
        }
        
        token = cstr_tok_next(&tok);
    }
    
    return current_knode;
}

uint32_t resolve_parent_path_to_address(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return 0xFFFFFFFF;
    }
    
    char parent_path[256];
    const char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        strcpy(parent_path, ".");
    }
    else if (last_slash == path) {
        strcpy(parent_path, "/");
    } 
    else {
        size_t parent_len = last_slash - path;
        if (parent_len >= sizeof(parent_path)) {
            return 0xFFFFFFFF;
        }
        
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }
    
    return resolve_path_to_address(parent_path);
}

bool vfs_parse_path(const char* path, uint16_t flags, uint32_t* out_knode, uint32_t* out_fs_node, bool* out_in_fs, struct FSDeviceContext** out_ctx) {
    uint32_t current_knode = knode_get_root();
    uint32_t current_fs_node = 0; 
    bool in_file_system = false;
    struct FSDeviceContext* current_ctx = NULL;
    
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
                struct FSDeviceContext* device_context = (struct FSDeviceContext*)knode_get_reference(current_knode, 1);
                
                if (device_address != KNODE_NULL && device_address != 0) {
                    struct FSPartitionBlock partition;
                    fs_device_get_partition(device_context, &partition);
                    if (device_context->is_open == true) {
                        current_fs_node = partition.root_directory;
                        current_ctx = device_context;
                        in_file_system = true;
                    }
                }
            }
        } else {
            if (strcmp(token, "..") == 0) {
                uint32_t parent = fs_directory_get_parent(current_ctx, current_fs_node);
                struct FSPartitionBlock partition;
                fs_device_get_partition(current_ctx, &partition);
                if (current_fs_node == partition.root_directory) {
                    in_file_system = false;
                    current_fs_node = 0;
                    current_ctx = NULL;
                    current_knode = knode_get_parent(current_knode);
                } else {
                    current_fs_node = parent;
                }
            } else {
                uint32_t ref_count = fs_directory_get_reference_count(current_ctx, current_fs_node);
                uint32_t found_ref = 0;
                
                for (uint32_t i = 0; i < ref_count; i++) {
                    uint32_t reference = fs_directory_get_reference(current_ctx, current_fs_node, i);
                    if (reference == FS_NULL) continue;
                    
                    char item_name[16]; 
                    fs_file_get_name(current_ctx, reference, item_name);
                    
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
                        uint32_t new_file_address = fs_file_create(current_ctx, last_token, default_perms, 0, parent_fs_node);
                        
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
    if (out_ctx) *out_ctx = current_ctx;
    return true;
}
