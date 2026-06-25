#ifndef _VIRTUAL_FILE_SYSTEM_H_
#define _VIRTUAL_FILE_SYSTEM_H_

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#define PATH_TOKEN_MAX              64
#define INVALID_FILE_ID              0

#define VFS_OPEN_READ             0x01
#define VFS_OPEN_WRITE            0x02
#define VFS_OPEN_CREATE           0x04

#define VFS_PERMISSION_EXECUTE    0x01
#define VFS_PERMISSION_READ       0x02
#define VFS_PERMISSION_WRITE      0x04

typedef uint32_t File;

// Open/close

File vfs_open(const char* path, uint16_t flags);
void vfs_close(File file);

// Read/write

void vfs_read(File file, void* buffer, uint32_t size);
void vfs_write(File file, const void* buffer, uint32_t size);

// File system

uint32_t vfs_get_size(File file);

bool vfs_set_permissions(File file, uint8_t perm);
bool vfs_get_permissions(File file, uint8_t* perm);

bool vfs_is_directory(const char* path);
bool vfs_is_directory_mounted(const char* path);

void vfs_mkfile(const char* path, uint32_t size);
void vfs_mkdir(const char* path);
void vfs_remove(const char* path);

uint32_t resolve_path_to_address(const char* path);

#endif
