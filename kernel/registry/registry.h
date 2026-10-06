#ifndef _KERNEL_REGISTRY_H_
#define _KERNEL_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/vfs/vfs.h>

#define REGISTRY_PERMISSION_READ        0x01
#define REGISTRY_PERMISSION_WRITE       0x02

#define REGISTRY_MAX_NAME_LEN     16
#define REGISTRY_MAX_PATH_LEN    128

// Limits applied when importing a hive file (protects against corrupt/hostile files)
#define REGISTRY_MAX_VALUE_SIZE   (64U * 1024U)
#define REGISTRY_MAX_DEPTH        32

struct RegistryValue {
    char name[REGISTRY_MAX_NAME_LEN];
    struct RegistryValue* next;        // Sibling values under the same key
    void* data;                        // Dynamic data payload
    size_t data_len;                   // Length of the payload
    uint16_t permissions;
};

struct RegistryKey {
    char name[REGISTRY_MAX_NAME_LEN];
    struct RegistryKey* next;          // Sibling keys under the same parent
    struct RegistryKey* child_keys;    // Head of child keys list
    struct RegistryValue* values;      // Head of values list inside this key
    uint16_t permissions;
};

struct RegistryHive {
    struct RegistryKey* root;
};

// registry_get/set_permissions() accept either a RegistryKey* or a RegistryValue*
// and read the field through RegistryKey. That only works while the field sits
// at the same offset in both structs, so enforce it at compile time.
_Static_assert(offsetof(struct RegistryKey, permissions) == offsetof(struct RegistryValue, permissions),
               "RegistryKey/RegistryValue permissions must share an offset");

extern struct RegistryHive hkey_root;
extern struct RegistryHive hkey_user;

bool registry_hive_initiate(const char* path);

struct RegistryKey* registry_create_key(struct RegistryKey* parent, const char* name, uint16_t permissions);
struct RegistryValue* registry_create_value(struct RegistryKey* parent, const char* name, uint16_t permissions, const void* data, size_t size);
void registry_free_key(struct RegistryKey* key);

uint16_t registry_get_permissions(void* ptr);
void registry_set_permissions(void* ptr, uint16_t permissions);

struct RegistryValue* registry_get_value(struct RegistryKey* parent, const char* name);
struct RegistryKey* registry_get_key(struct RegistryKey* parent, const char* name);

bool registry_hive_import(struct RegistryHive* hive, const char* path);
bool registry_hive_export(struct RegistryHive* hive, const char* path);

// Write both hives back to the files they were loaded from by
// registry_hive_initiate(). Returns false if either hive failed to save
// (or the registry was never initiated).
bool registry_hive_save_all(void);

#endif


