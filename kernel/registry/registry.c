#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/util/string.h>

#include <kernel/registry/registry.h>

// Assuming your slab/fallback framework provides these standard library hooks:
extern void* malloc(size_t size);
extern void  free(void* ptr);

// Head of the registered objects tracked list
static struct RegistryNode* registry_head = NULL;

// Find a tracked pointer's metadata node
static struct RegistryNode* find_node(void* ptr) {
    if (!ptr) return NULL;
    
    struct RegistryNode* current = registry_head;
    while (current != NULL) {
        if (current->target_address == ptr) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void* registry_alloc(size_t size, uint8_t type, uint8_t flags, uint8_t perm) {
    if (size == 0) return NULL;
    
    // Allocate the actual data buffer using the unified allocator
    void* data_ptr = malloc(size);
    if (!data_ptr) return NULL;
    
    // Allocate the tracking node from the allocator as well
    struct RegistryNode* tracker = (struct RegistryNode*)malloc(sizeof(struct RegistryNode));
    if (!tracker) {
        free(data_ptr); // Clean up to avoid memory leaks
        return NULL;
    }
    
    // Populate metadata properties
    tracker->target_address = data_ptr;
    tracker->size           = size;
    tracker->type           = type;
    tracker->flags          = flags;
    tracker->perm           = perm;
    
    // Link it into our global tracking list
    tracker->next = registry_head;
    registry_head = tracker;
    
    return data_ptr;
}

void registry_free(void* ptr) {
    if (!ptr) return;
    
    struct RegistryNode* current = registry_head;
    struct RegistryNode* previous = NULL;
    
    while (current != NULL) {
        if (current->target_address == ptr) {
            // Unlink from the linked list
            if (previous == NULL) {
                registry_head = current->next;
            } else {
                previous->next = current->next;
            }
            
            // Free the actual data buffer
            free(current->target_address);
            
            // Free the tracking node structure
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

uint8_t registry_get_type(void* ptr) {
    struct RegistryNode* node = find_node(ptr);
    return node ? node->type : 0;
}

void registry_set_type(void* ptr, uint8_t type) {
    struct RegistryNode* node = find_node(ptr);
    if (node) node->type = type;
}

uint8_t registry_get_flags(void* ptr) {
    struct RegistryNode* node = find_node(ptr);
    return node ? node->flags : 0;
}

void registry_set_flags(void* ptr, uint8_t flags) {
    struct RegistryNode* node = find_node(ptr);
    if (node) node->flags = flags;
}

uint8_t registry_get_permissions(void* ptr) {
    struct RegistryNode* node = find_node(ptr);
    return node ? node->perm : 0;
}

void registry_set_permissions(void* ptr, uint8_t permissions) {
    struct RegistryNode* node = find_node(ptr);
    if (node) node->perm = permissions;
}

size_t registry_get_size(void* ptr) {
    struct RegistryNode* node = find_node(ptr);
    return node ? node->size : 0;
}
