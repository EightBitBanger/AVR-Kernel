#ifndef _VIRTUAL_FILE_SYSTEM_H_
#define _VIRTUAL_FILE_SYSTEM_H_

#define PATH_TOKEN_MAX              64
#define VFS_INVALID_FILE             0

#define VFS_OPEN_READ             0x01
#define VFS_OPEN_WRITE            0x02
#define VFS_OPEN_CREATE           0x04

#define VFS_PERMISSION_EXECUTE    0x01
#define VFS_PERMISSION_READ       0x02
#define VFS_PERMISSION_WRITE      0x04

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t File;

typedef struct {
    uint32_t size;
    
    uint8_t permissions;
    uint32_t certificate;
    
} FSFileStats;

// Device
uint64_t vfs_device_get_capacity(const char* path);
uint64_t vfs_device_get_used(const char* path);

// File
File vfs_open(const char* path, uint16_t flags);
void vfs_close(File file);

int32_t vfs_read(File file, void* buffer, uint32_t size);
int32_t vfs_write(File file, const void* buffer, uint32_t size);

uint32_t vfs_seek(File file, uint32_t position);
uint32_t vfs_tell(File file);

uint32_t vfs_get_size(File file);

// File system
bool vfs_mkdir(const char* path);
bool vfs_remove(const char* path);
bool vfs_rename(const char* path, const char* name);
bool vfs_exists(const char* path);
bool vfs_truncate(const char* path, uint32_t new_size);

// Operations
bool vfs_stat(const char* path, FSFileStats* stats);

bool vfs_set_permissions(const char* path, uint8_t perm);
bool vfs_get_permissions(const char* path, uint8_t* perm);
bool vfs_is_directory(const char* path);
bool vfs_directory_check_mounted(const char* path);

// Cryptography
bool vfs_file_get_certificate(const char* path, uint32_t certificate);

// Directory
bool vfs_directory_check(const char* path);

uint32_t vfs_directory_get_item_count(const char* path);
bool vfs_directory_get_item(const char* path, unsigned int index, char* name_out);

#endif
