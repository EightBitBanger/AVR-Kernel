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
    strncpy(new_event->name, name, KEVENT_NAME_LENGTH_MAX);
    new_event->name[KEVENT_NAME_LENGTH_MAX-1] = '\0'; 
    
    strncpy(new_event->args, arguments, KEVENT_ARG_LENGTH_MAX);
    new_event->args[KEVENT_ARG_LENGTH_MAX-1] = '\0';
    
    new_event->flags = flags;
    
    // Append the KEvent pointer to the linked list
    if (!list_append(&event_list_head, &event_list_tail, (void*)new_event)) {
        free(new_event);
        return false;
    }
    
    return true;
}

bool kernel_event_remove_by_ptr(struct KEvent* target_event) {
    if (target_event == NULL) return false;
    
    // Pass the actual event payload pointer.
    if (!list_remove(&event_list_head, &event_list_tail, (void*)target_event)) {
        return false; 
    }
    
    // Safely free the actual event data structure
    free(target_event);
    return true;
}

bool kernel_event_remove(const char* name) {
    if (event_list_head == NULL) return false;
    
    struct list_node* current = event_list_head;
    
    // Find the event matching the name string
    while (current != NULL) {
        struct KEvent* ev = (struct KEvent*)current->data;
        if (strncmp(ev->name, name, KEVENT_NAME_LENGTH_MAX) == 0) {
            // Hand off the found KEvent pointer to our safe removal function
            return kernel_event_remove_by_ptr(ev);
        }
        current = current->next;
    }
    
    return false;
}

void kernel_event_update(void) {
    struct list_node* current = event_list_head;
    
    // Process events
    while (current != NULL) {
        // Cache next pointer because we break or flag modifications mid-stream
        struct list_node* next_node = current->next;
        struct KEvent* event = (struct KEvent*)current->data;
        
        // Execute program
        if (event->flags & KEVENT_EXECUTE) {
            event->flags &= ~KEVENT_EXECUTE;
            
            // Explorer
            
            if (strncmp(event->name, "explorer", KEVENT_NAME_LENGTH_MAX) == 0) {
                
                explorer_main(event->args);
            } 
            
            // Notepad
            
            else if (strncmp(event->name, "notepad", KEVENT_NAME_LENGTH_MAX) == 0) {
                
                notepad_main(event->args);
            }
            
            // Sweep up this old event
            event->flags |= KEVENT_DEAD;
            
            break; // Stop processing further events
        }
        
        // Refresh the desktop window manager
        if (event->flags & KEVENT_DWM_REFRESH) {
            event->flags &= ~KEVENT_DWM_REFRESH;
            
            dwm_event_send(DWM_EVENT_REFRESH);
            
            // Sweep up this old event
            event->flags |= KEVENT_DEAD;
            
            break; // Stop processing further events
        }
        
        // Unknown event
        event->flags |= KEVENT_DEAD;
        
        current = next_node;
    }
    
    // Flush all accumulated dead events
    struct list_node* dead = event_list_head;
    
    while (dead != NULL) {
        // Cache the next node before we potentially destroy 'current'
        struct list_node* next_node = dead->next;
        struct KEvent* event = (struct KEvent*)dead->data;
        
        if (event->flags & KEVENT_DEAD) {
            
            list_remove(&event_list_head, &event_list_tail, (void*)event);
            
            free(event);
        }
        
        dead = next_node;
    }
}
