
#ifndef BASE_DIRECTORY_H
#define BASE_DIRECTORY_H

#include <kernel/fs/fs.h>

uint32_t fs_directory_create(struct FSDeviceContext* ctx, const char* name, uint8_t permissions, uint32_t parent_directory);
// Frees only the directory's own header and extents; entries are NOT freed.
// Use fs_directory_delete_recursive to remove a directory with its contents.
bool     fs_directory_delete(struct FSDeviceContext* ctx, uint32_t address);

// Unlinks the directory from parent_directory (if not FS_NULL), then frees
// every file and subdirectory below it and finally the directory itself.
// Refuses the partition root. Does not check permissions.
bool     fs_directory_delete_recursive(struct FSDeviceContext* ctx, uint32_t address, uint32_t parent_directory);

uint8_t  fs_directory_add_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address);
uint8_t  fs_directory_remove_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address);
uint32_t fs_directory_get_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t index);
uint32_t fs_directory_get_reference_count(struct FSDeviceContext* ctx, uint32_t directory_address);

uint32_t fs_directory_find(struct FSDeviceContext* ctx, uint32_t directory_address, const char* name);
uint32_t fs_directory_get_parent(struct FSDeviceContext* ctx, uint32_t directory_address);

#endif

