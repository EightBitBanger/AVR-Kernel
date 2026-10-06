#ifndef _VIRTUAL_FILE_SYSTEM_H_
#define _VIRTUAL_FILE_SYSTEM_H_

#define PATH_TOKEN_MAX              64
#define VFS_INVALID_FILE             0

// Longest file/directory name (in characters, without the terminator) that
// can be stored. Must equal FS_NAME_LENGTH_MAX - 1 (checked in vfs_file.c).
#define VFS_NAME_MAX                15

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

// Locking
//
// Serializes all VFS and mounted-filesystem access. Every vfs_* function
// below takes it internally, so ordinary callers never need it. Take it
// yourself only to:
//   - make several vfs_* calls atomic (e.g. get_item_count + get_item loop)
//   - call fs_* directly on a mounted device (formatting, mounting, tools)
//   - modify mount knodes outside the VFS
// Recursive per thread. Thread context only, never from an IRQ handler.
// If kernel_big_lock is also needed, take it first.
void vfs_lock(void);
void vfs_unlock(void);

// Device
uint64_t vfs_device_get_capacity(const char* path);
uint64_t vfs_device_get_used(const char* path);

// Flush every mounted storage device (allocation bitmap, sector cache and
// the drive's write cache). Call before power-off to avoid corruption.
// Returns false if any device failed to flush.
bool vfs_sync_all(void);

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

// Copy a file, or a directory with everything inside it, to `dest` (the full
// new path, which must not exist yet). Permissions are carried over. Refuses
// mounted devices and copying a directory into itself. On failure nothing is
// left at `dest`. See vfs_copy.c.
bool vfs_copy(const char* src, const char* dest);

// Operations
bool vfs_stat(const char* path, FSFileStats* stats);

bool vfs_set_permissions(const char* path, uint8_t perm);
bool vfs_get_permissions(const char* path, uint8_t* perm);
bool vfs_directory_check(const char* path);
bool vfs_directory_check_mounted(const char* path);

// Cryptography
bool vfs_file_get_certificate(const char* path, uint32_t certificate);

// Directory
uint32_t vfs_directory_get_item_count(const char* path);
bool vfs_directory_get_item(const char* path, unsigned int index, char* name_out);

#endif
