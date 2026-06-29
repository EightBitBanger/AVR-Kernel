#ifndef _VIRTUAL_FILE_SYSTEM_H_
#define _VIRTUAL_FILE_SYSTEM_H_

#define PATH_TOKEN_MAX              64
#define INVALID_FILE_ID              0

#define VFS_OPEN_READ             0x01
#define VFS_OPEN_WRITE            0x02
#define VFS_OPEN_CREATE           0x04

#define VFS_PERMISSION_EXECUTE    0x01
#define VFS_PERMISSION_READ       0x02
#define VFS_PERMISSION_WRITE      0x04

typedef uint32_t File;

// Device

// Get the capacity of a mounted file system
uint64_t vfs_device_get_capacity(const char* path);

// Get the amount of used space in a mounted file system
uint64_t vfs_device_get_used(const char* path);

// File

File vfs_open(const char* path, uint16_t flags);
void vfs_close(File file);

bool vfs_file_read(File file, void* buffer, uint32_t size);
bool vfs_file_write(File file, const void* buffer, uint32_t size);

uint32_t vfs_file_get_size(File file);

// File system

bool vfs_mkfile(const char* path, uint32_t size);
bool vfs_mkdir(const char* path);
bool vfs_remove(const char* path);
bool vfs_rename(const char* path, const char* name);

bool vfs_exists(const char* path);

bool vfs_truncate(const char* path, uint32_t new_size);

// Operations

bool vfs_set_permissions(const char* path, uint8_t perm);
bool vfs_get_permissions(const char* path, uint8_t* perm);

bool vfs_is_directory(const char* path);
bool vfs_is_directory_mounted(const char* path);

// Directory

uint32_t vfs_directory_count(const char* path);

#endif
