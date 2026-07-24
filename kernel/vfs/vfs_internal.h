#ifndef _VIRTUAL_FILE_SYSTEM_INTERNAL_H_
#define _VIRTUAL_FILE_SYSTEM_INTERNAL_H_

#include <kernel/vfs/vfs.h>

typedef struct {
    File id;              // Unique integer ID returned to the user
    uint32_t address;     // Resolved knode address or fs_node address
    bool in_file_system;  // Distinguishes between knode and block file system
    uint32_t offset;      // Positions tracker for virtual knodes
    FileHandle handle;    // Embedded File system handle for physical file system IO
    uint16_t flags;       // Flags relating to the state of the opened file
} OpenFileDescriptor;

extern struct list_node* open_files_head;
extern struct list_node* open_files_tail;
extern File next_unique_id;

uint32_t resolve_path_to_address(const char* path);
uint32_t resolve_parent_path_to_address(const char* path);
bool vfs_parse_path(const char* path, uint16_t flags, uint32_t* out_knode, uint32_t* out_fs_node, bool* out_in_fs);

OpenFileDescriptor* vfs_file_find_open(File id);

bool vfs_directory_check_mounted(const char* path);

#endif
