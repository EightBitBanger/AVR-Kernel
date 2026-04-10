#include <kernel/kernel.h>
#include <string.h>
#include <stdint.h>

uint32_t create_knode(const char* name, uint32_t parent_address);
void     destroy_knode(uint32_t address);

uint8_t knode_add_reference(uint32_t directory_address, uint32_t reference_ptr);
uint8_t knode_remove_reference(uint32_t directory_address, uint32_t reference_ptr);
uint32_t knode_get_reference(uint32_t directory_address, uint32_t index);
uint32_t knode_find_by_name(const char* name);

uint32_t create_extent(void);
void     destroy_extent(uint32_t address);

uint32_t knode_find_in(uint32_t directory_address, const char* name);
uint32_t knode_get_parent(uint32_t directory_address);
uint32_t knode_get_root(void);
