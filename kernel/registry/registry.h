#ifndef KERNEL_REGISTRY_H
#define KERNEL_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#define REGISTRY_FLAG_DIRTY       0x01
#define REGISTRY_FLAG_MOUNT       0x02
#define REGISTRY_FLAG_DIRECTORY   0x04
#define REGISTRY_FLAG_EXTENT      0x08

#define REGISTRY_PERMISSION_EXECUTABLE  0x01
#define REGISTRY_PERMISSION_READ        0x02
#define REGISTRY_PERMISSION_WRITE       0x04
#define REGISTRY_PERMISSION_USER        0x08

#define REGISTRY_TYPE_DEVICE            0x01
#define REGISTRY_TYPE_DRIVER            0x02
#define REGISTRY_TYPE_BLOCK             0x04
#define REGISTRY_TYPE_RAW               0x08
#define REGISTRY_TYPE_PROCBLOCK         0x10

struct RegistryNode {
    void* target_address;             // The pointer returned by malloc()
    struct RegistryNode* next;        // Linked list link
    size_t            size;           // Size of the allocation
    uint8_t           flags;          // Subsystem flags
    uint8_t           type;           // Subsystem type
    uint8_t           perm;           // Subsystem permissions
};

// Core API
void* registry_alloc(size_t size, uint8_t type, uint8_t flags, uint8_t perm);
void   registry_free(void* ptr);

// Metadata Accessors
uint8_t  registry_get_type(void* ptr);
void     registry_set_type(void* ptr, uint8_t type);
uint8_t  registry_get_flags(void* ptr);
void     registry_set_flags(void* ptr, uint8_t flags);
uint8_t  registry_get_permissions(void* ptr);
void     registry_set_permissions(void* ptr, uint8_t permissions);
size_t   registry_get_size(void* ptr);

#endif
