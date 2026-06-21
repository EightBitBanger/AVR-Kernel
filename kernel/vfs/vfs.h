#ifndef _VIRTUAL_FILE_SYSTEM_H_
#define _VIRTUAL_FILE_SYSTEM_H_

#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#define PATH_TOKEN_MAX 64
#define INVALID_FILE_ID 0

#define VFS_OPEN_READ    0x01
#define VFS_OPEN_WRITE   0x02
#define VFS_OPEN_CREATE  0x04
#define VFS_OPEN_APPEND  0x08

typedef uint32_t File;

typedef struct {
    File id;              // Unique integer ID returned to the user
    uint32_t address;     // Resolved knode address or fs_node address
    bool in_file_system;  // Distinguishes between knode and block file system
    uint32_t offset;      // Positions tracker for virtual knodes
    FileHandle handle;    // Embedded File system handle for physical file system IO
    uint16_t flags;       // Flags relating to the state of the opened file
} OpenFileDescriptor;

// Open/close

File vfs_open(const char* path, uint16_t flags);
void vfs_close(File file);

// Read/write

void vfs_read(File file, void* buffer, uint32_t size);
void vfs_write(File file, const void* buffer, uint32_t size);

// File system

void vfs_mkdir(const char* path);
void vfs_rm(const char* path);

uint32_t resolve_path_to_address(const char* path);

#endif
