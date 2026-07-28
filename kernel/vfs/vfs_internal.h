#ifndef _VIRTUAL_FILE_SYSTEM_INTERNAL_H_
#define _VIRTUAL_FILE_SYSTEM_INTERNAL_H_

#include <kernel/vfs/vfs.h>

typedef struct {
    File id;
    uint32_t address;
    bool in_file_system;
    uint32_t offset;
    FileHandle handle;
    uint16_t flags;
    struct FSDeviceContext* ctx;
} OpenFileDescriptor;

extern struct list_node* open_files_head;
extern struct list_node* open_files_tail;
extern File next_unique_id;

uint32_t resolve_path_to_address(const char* path);
uint32_t resolve_parent_path_to_address(const char* path);
uint32_t resolve_path_to_mount_point(const char* path);
bool vfs_parse_path(const char* path, uint16_t flags, uint32_t* out_knode, uint32_t* out_fs_node, bool* out_in_fs, struct FSDeviceContext** out_ctx);
struct FSDeviceContext* vfs_device_get_context(const char* path);
uint8_t* vfs_device_get_block(const char* path);

OpenFileDescriptor* vfs_file_find_open(File id);

bool vfs_directory_check_mounted(const char* path);

#endif
