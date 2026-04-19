#ifndef BASE_DIRECTORY_H
#define BASE_DIRECTORY_H

#include <kernel/fs/fs.h>

uint32_t fs_directory_create(const char* name, uint8_t permissions, uint32_t parent_directory);
void     fs_directory_delete(uint32_t address);

uint8_t  fs_directory_add_reference(uint32_t directory_address, uint32_t reference_address);
uint8_t  fs_directory_remove_reference(uint32_t directory_address, uint32_t reference_address);
uint32_t fs_directory_get_reference(uint32_t directory_address, uint32_t index);
uint32_t fs_directory_get_reference_count(uint32_t directory_address);

uint32_t fs_directory_find(uint32_t directory_address, const char* name);
uint32_t fs_directory_get_parent(uint32_t directory_address);

#endif
