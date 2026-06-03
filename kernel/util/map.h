#ifndef KERNEL_UTIL_MAP_H
#define KERNEL_UTIL_MAP_H

#include <stddef.h>
#include <stdbool.h>

#ifdef KERNEL_PLATFORM_X86
  #include <kernel/arch/x86/heap.h>
#endif

#ifdef KERNEL_PLATFORM_AVR
  #include <kernel/arch/avr/heap.h>
#endif

struct map_node {
    char name[16];
    void* data;
    struct map_node* next;
    struct map_node* prev;
};

static inline bool map_append(struct map_node** head, struct map_node** tail, void* data) {
    struct map_node* new_node = malloc(sizeof(struct map_node));
    if (new_node == NULL) return false;
    
    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = NULL;
    
    if (*head == NULL) {
        *head = new_node;
        *tail = new_node;
    } else {
        new_node->prev = *tail;
        (*tail)->next = new_node;
        *tail = new_node;
    }
    return true;
}

static inline bool map_remove(struct map_node** head, struct map_node** tail, void* target_data) {
    if (*head == NULL) return false;
    
    struct map_node* current = *head;
    
    while (current != NULL && current->data != target_data) {
        current = current->next;
    }
    
    if (current == NULL) return false;
    
    if (current->prev != NULL) {
        current->prev->next = current->next;
    } else {
        *head = current->next;
    }
    
    if (current->next != NULL) {
        current->next->prev = current->prev;
    } else {
        *tail = current->prev;
    }
    
    free(current);
    return true;
}

#endif
