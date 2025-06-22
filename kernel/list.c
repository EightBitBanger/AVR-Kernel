#include <avr/io.h>

#include <kernel/list.h>

struct Node* ListCreateNode(void* data) {
    
    struct Node* newNode = (struct Node*) malloc(sizeof(struct Node));
    
    if (!newNode) 
        return NULL;
    
    newNode->data = data;
    newNode->next = NULL;
    
    return newNode;
}

void ListAddNode(struct Node** head, void* data) {
    
    struct Node* newNode = ListCreateNode(data);
    
    // If the list is empty, make new node the head
    if (*head == NULL) {
        
        *head = newNode;
        
    } else {
        
        struct Node* temp = *head;
        
        // Get the last node
        while (temp->next != NULL) {
            temp = temp->next;
        }
        
        // Add the new node at the end
        temp->next = newNode;
    }
    
    return;
}

void ListRemoveNode(struct Node** head, void* data) {
    if (*head == NULL) 
        return;
    
    struct Node* temp = *head;
    struct Node* prev = NULL;
    
    // If the node to be removed is the head node
    if (temp != NULL && temp->data == data) {
        *head = temp->next;  // Move head to the next node
        free(temp);          // Free the node
        return;
    }
    
    // Find the node to be removed
    while (temp != NULL && temp->data != data) {
        prev = temp;
        temp = temp->next;
    }
    
    // Node not found
    if (temp == NULL) {
        return;
    }
    
    // Unlink the node from the list
    prev->next = temp->next;
    // Free memory of the node
    free(temp);
    
    return;
}

// Function to free the allocated memory for the list
void ListDelete(struct Node* head) {
    
    struct Node* temp;
    
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
    
    return;
}

uint32_t ListGetSize(struct Node* head) {
    int size = 0;
    struct Node* temp = head;
    
    // Count the nodes
    while (temp != NULL) {
        size++;
        temp = temp->next;
    }
    
    return size;
}

struct Node* ListGetNode(struct Node* head, int index) {
    int currentIndex = 0;
    struct Node* temp = head;
    
    // Find the node index
    while (temp != NULL) {
        
        // Find the node at index
        if (currentIndex == index) 
            return temp;
        
        currentIndex++;
        temp = temp->next;
    }
    
    // Index out of range
    return NULL;
}


