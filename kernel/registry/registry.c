#include <kernel/memory/malloc.h>

#include <kernel/registry/registry.h>
#include <kernel/util/string.h>

struct RegistryHive hkey_root = { NULL };
struct RegistryHive hkey_user = { NULL };
struct RegistryHive hkey_admin = { NULL };

static void registry_free_value(struct RegistryValue* val) {
    if (!val) return;
    if (val->data) {
        free(val->data);
    }
    free(val);
}

struct RegistryKey* registry_create_key(struct RegistryKey* parent, const char* name, uint16_t permissions) {
    if (!name) return NULL;

    struct RegistryKey* new_key = (struct RegistryKey*)malloc(sizeof(struct RegistryKey));
    if (!new_key) return NULL;

    strncpy(new_key->name, name, REGISTRY_MAX_NAME_LEN);
    new_key->permissions = permissions;
    new_key->next = NULL;
    new_key->child_keys = NULL;
    new_key->values = NULL;

    // Link into parent branch
    if (parent) {
        new_key->next = parent->child_keys;
        parent->child_keys = new_key;
    }

    return new_key;
}

struct RegistryValue* registry_create_value(struct RegistryKey* parent, const char* name, uint16_t permissions, const void* data, size_t size) {
    if (!parent || !name) return NULL;

    struct RegistryValue* new_val = (struct RegistryValue*)malloc(sizeof(struct RegistryValue));
    if (!new_val) return NULL;

    strncpy(new_val->name, name, REGISTRY_MAX_NAME_LEN);
    new_val->permissions = permissions;
    new_val->data_len = size;
    new_val->next = NULL;

    if (data && size > 0) {
        new_val->data = malloc(size);
        if (!new_val->data) {
            free(new_val);
            return NULL;
        }
        memcpy(new_val->data, data, size);
    } else {
        new_val->data = NULL;
    }

    // Link into parent key's value list
    new_val->next = parent->values;
    parent->values = new_val;

    return new_val;
}

void registry_free_key(struct RegistryKey* key) {
    if (!key) return;

    // Recursively clean out all subkeys
    struct RegistryKey* current_child = key->child_keys;
    while (current_child) {
        struct RegistryKey* next_child = current_child->next;
        registry_free_key(current_child);
        current_child = next_child;
    }

    // Purge allocations for all attached values
    struct RegistryValue* current_val = key->values;
    while (current_val) {
        struct RegistryValue* next_val = current_val->next;
        registry_free_value(current_val);
        current_val = next_val;
    }

    // Free the root of this key block
    free(key);
}

uint16_t registry_get_permissions(void* ptr) {
    if (!ptr) return 0;
    return ((struct RegistryKey*)ptr)->permissions; 
}

void registry_set_permissions(void* ptr, uint16_t permissions) {
    if (!ptr) return;
    ((struct RegistryKey*)ptr)->permissions = permissions;
}

static void registry_export_key_recursive(struct RegistryKey* key, void* file_handle) {
    if (!key) return;
    
    // Write the key metadata out to disk
    // vfs_write(file_handle, &key->name, REGISTRY_MAX_NAME_LEN);
    // vfs_write(file_handle, &key->permissions, sizeof(key->permissions));
    
    // Count and write the number of values directly under this key
    uint32_t val_count = 0;
    struct RegistryValue* v = key->values;
    while (v) {
        val_count++;
        v = v->next;
    }
    // vfs_write(file_handle, &val_count, sizeof(val_count));
    
    // Serialize each value associated with this key
    v = key->values;
    while (v) {
        // vfs_write(file_handle, &v->name, REGISTRY_MAX_NAME_LEN);
        // vfs_write(file_handle, &v->permissions, sizeof(v->permissions));
        // vfs_write(file_handle, &v->data_len, sizeof(v->data_len));
        if (v->data_len > 0 && v->data) {
            // vfs_write(file_handle, v->data, v->data_len);
        }
        v = v->next;
    }
    
    // Count and write the number of subkeys to know how many branches follow
    uint32_t key_count = 0;
    struct RegistryKey* child = key->child_keys;
    while (child) {
        key_count++;
        child = child->next;
    }
    // vfs_write(file_handle, &key_count, sizeof(key_count));
    
    // Recursively export all child branches deeper in the tree
    child = key->child_keys;
    while (child) {
        registry_export_key_recursive(child, file_handle);
        child = child->next;
    }
}

bool registry_hive_import(const char* path) {
    if (!path) return false;

    // Open file block example
    // void* file = vfs_open(path, VFS_MODE_READ);
    // if (!file) return false;

    // Placeholder layout loop for parsing your file back into memory:
    // uint32_t total_keys;
    // vfs_read(file, &total_keys, sizeof(total_keys));
    // loop... read elements, reconstruct structure topology via registry_create_key

    // vfs_close(file);
    return true;
}

bool registry_hive_export(const char* path) {
    if (!path) return false;

    // Mock open file system call
    // void* file = vfs_open(path, VFS_MODE_WRITE | VFS_MODE_CREATE);
    // if (!file) return false;

    // Export our default master root key hive tree
    if (hkey_root.root) {
        // Example handling: registry_export_key_recursive(hkey_root.root, file);
    }

    // Close down file descriptors
    // vfs_close(file);
    return true;
}
