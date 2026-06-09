#include <kernel/dwm/dwm.h>
#include <kernel/util/string.h>

#include <kernel/util/list.h>
#include <kernel/util/map.h>

extern struct map_node* resource_head;
extern struct map_node* resource_tail;

void dwm_resource_load(const char* name, void* resource) {
    if (name == NULL || resource == NULL) return;
    
    if (map_append(&resource_head, &resource_tail, resource)) {
        strncpy(resource_tail->name, name, sizeof(resource_tail->name) - 1);
        resource_tail->name[sizeof(resource_tail->name) - 1] = '\0';
    }
}

void* dwm_resource_find(const char* name) {
    if (name == NULL) return NULL;
    
    struct map_node* current = resource_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->data;
        }
        current = current->next;
    }
    
    return NULL;
}

void dwm_resource_unload(const char* name) {
    if (name == NULL) return;

    struct map_node* current = resource_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            map_remove(&resource_head, &resource_tail, current->data);
            return;
        }
        current = current->next;
    }
}
