#ifndef BASE_DIRECTORY_H
#define BASE_DIRECTORY_H

#include <kernel/fs/fs.h>

uint32_t fs_directory_create(struct FSDeviceContext* ctx, const char* name, uint8_t permissions, uint32_t parent_directory);
bool     fs_directory_delete(struct FSDeviceContext* ctx, uint32_t address);

uint8_t  fs_directory_add_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address);
uint8_t  fs_directory_remove_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address);
uint32_t fs_directory_get_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t index);
uint32_t fs_directory_get_reference_count(struct FSDeviceContext* ctx, uint32_t directory_address);

uint32_t fs_directory_find(struct FSDeviceContext* ctx, uint32_t directory_address, const char* name);
uint32_t fs_directory_get_parent(struct FSDeviceContext* ctx, uint32_t directory_address);

#endif
