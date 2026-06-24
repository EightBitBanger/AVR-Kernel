#include <stdio.h>
#include <stdbool.h>
#include <kernel/util/string.h>

#include <kernel/dwm/dwm.h>
#include <kernel/util/list.h>
#include <kernel/device/kevent.h>
#include <kernel/events.h>

struct list_node* event_list_head = NULL;
struct list_node* event_list_tail = NULL;

bool kernel_event_send(uint8_t flags, const char* name, const char* arguments) {
    // Allocate memory for the actual KEvent object
    struct KEvent* new_event = malloc(sizeof(struct KEvent));
    if (new_event == NULL) 
        return false;
    
    // Initialize the KEvent fields and guarantee null-termination
    strncpy(new_event->name, name, 32);
    new_event->name[31] = '\0'; 
    
    strncpy(new_event->args, arguments, 32);
    new_event->args[31] = '\0';
    
    new_event->flags = flags;
    
    // Append the KEvent pointer to the linked list
    if (!list_append(&event_list_head, &event_list_tail, (void*)new_event)) {
        free(new_event);
        return false;
    }
    
    return true;
}

bool kernel_event_remove(const char* name) {
    if (event_list_head == NULL) return false;
    
    struct list_node* current = event_list_head;
    struct list_node* target_node = NULL;
    struct KEvent* target_event = NULL;
    
    // Walk the list nodes to find the KEvent matching the name
    while (current != NULL) {
        struct KEvent* ev = (struct KEvent*)current->data;
        if (strncmp(ev->name, name, 32) == 0) {
            target_node = current;
            target_event = ev;
            break;
        }
        current = current->next;
    }
    
    if (target_event == NULL) 
        return false;
    
    list_remove(&event_list_head, &event_list_tail, (void*)target_node);
    
    free(target_event);
    
    return true;
}

void kernel_event_update(void) {
    struct list_node* current = event_list_head;
    while (current != NULL) {
        struct KEvent* event = (struct KEvent*)current->data;
        
        if (event->flags & KEVENT_EXECUTE) {
            event->flags &= ~KEVENT_EXECUTE;
            
            if (strncmp(event->name, "explorer", 32) == 0) {
                explorer_main(event->args);
            }
            
            break;
        }
        
        current = current->next;
    }
}
