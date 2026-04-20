#ifndef KNODE_DIRECTORY_H
#define KNODE_DIRECTORY_H

#define KNODE_NAME_LENGTH_MAX       16
#define KNODE_NULL                  KMALLOC_NULL

#include <stdint.h>

uint32_t create_knode(const char* name, uint32_t parent_address);
void destroy_knode(uint32_t address, uint32_t parent_address);

void knode_get_name(uint32_t address, char* name);
void knode_set_name(uint32_t address, const char* name);

uint8_t knode_add_reference(uint32_t directory_address, uint32_t reference_ptr);
uint8_t knode_remove_reference(uint32_t directory_address, uint32_t reference_ptr);
uint32_t knode_get_reference(uint32_t directory_address, uint32_t index);
uint32_t knode_find_by_name(uint32_t directory_address, const char* name);

uint32_t create_extent(void);
void     destroy_extent(uint32_t address);

uint32_t knode_get_parent(uint32_t directory_address);
uint32_t knode_get_root(void);

#endif
