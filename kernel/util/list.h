#ifndef KERNEL_UTIL_LIST_H
#define KERNEL_UTIL_LIST_H

#include <stddef.h>
#include <stdbool.h>

#ifdef KERNEL_PLATFORM_X86
  #include <kernel/arch/x86/heap.h>
#endif

#ifdef KERNEL_PLATFORM_AVR
  #include <kernel/arch/avr/heap.h>
#endif

struct list_node {
    void* data;
    struct list_node* next;
    struct list_node* prev;
};

static inline bool list_append(struct list_node** head, struct list_node** tail, void* data) {
    struct list_node* new_node = malloc(sizeof(struct list_node));
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

static inline bool list_remove(struct list_node** head, struct list_node** tail, void* target_data) {
    if (*head == NULL) return false;
    
    struct list_node* current = *head;
    
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
