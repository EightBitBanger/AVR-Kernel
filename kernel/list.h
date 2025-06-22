#ifndef _SINGLY_LINKED_LIST__
#define _SINGLY_LINKED_LIST__

#include <stdio.h>
#include <stdlib.h>

struct Node {
    
    struct Node* next;
    
    void* data;
};

// Function to create a new node
struct Node* ListCreateNode(void* data);

// Function to add a node at the end of the list
void ListAddNode(struct Node** head, void* data);

// Function to remove a node with the given data from the list
void ListRemoveNode(struct Node** head, void* data);

// Function to free the allocated memory for the list
void ListDelete(struct Node* head);

// Function to get the size of the list
uint32_t ListGetSize(struct Node* head);

// Function to get a node at a specific index
struct Node* ListGetNode(struct Node* head, int index);

#endif
