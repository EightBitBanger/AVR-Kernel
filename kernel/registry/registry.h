#ifndef _KERNEL_REGISTRY_H_
#define _KERNEL_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define REGISTRY_PERMISSION_READ        0x01
#define REGISTRY_PERMISSION_WRITE       0x02

#define REGISTRY_MAX_NAME_LEN    16

struct RegistryValue;

struct RegistryKey {
    char name[REGISTRY_MAX_NAME_LEN];
    struct RegistryKey* next;          // Sibling keys under the same parent
    struct RegistryKey* child_keys;    // Head of child keys list
    struct RegistryValue* values;      // Head of values list inside this key
    uint16_t permissions;
};

struct RegistryValue {
    char name[REGISTRY_MAX_NAME_LEN];
    struct RegistryValue* next;        // Sibling values under the same key
    void* data;                        // Dynamic data payload
    size_t data_len;                   // Length of the payload (useful for future serialization)
    uint16_t permissions;
};

struct RegistryHive {
    struct RegistryKey* root;
};

extern struct RegistryHive hkey_root;
extern struct RegistryHive hkey_user;
extern struct RegistryHive hkey_admin;

struct RegistryKey* registry_create_key(struct RegistryKey* parent, const char* name, uint16_t permissions);
struct RegistryValue* registry_create_value(struct RegistryKey* parent, const char* name, uint16_t permissions, const void* data, size_t size);
void registry_free_key(struct RegistryKey* key);

uint16_t registry_get_permissions(void* ptr);
void registry_set_permissions(void* ptr, uint16_t permissions);

bool registry_hive_import(const char* path);
bool registry_hive_export(const char* path);

#endif
