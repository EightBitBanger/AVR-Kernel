#ifndef _VIRTUAL_FILE_SYSTEM_INTERNAL_H_
#define _VIRTUAL_FILE_SYSTEM_INTERNAL_H_

#include <kernel/vfs/vfs.h>
#include <kernel/fs/fs.h>

//
// Locking
//
// One recursive sleeping lock (vfs_lock / vfs_unlock, see vfs_lock.c)
// serializes the whole VFS:
//
//   - the open-file table (open_files_head/tail, next_unique_id) and every
//     OpenFileDescriptor in it
//   - path resolution and knode changes made through the VFS
//   - every mounted basefs device: its FSDeviceContext sector cache and
//     bitmap window, and the on-disk structures behind them
//
// basefs is deliberately NOT thread-safe (it also builds for AVR, which has
// no scheduler). Its contract is: all fs_* calls on a mounted device must be
// made with the VFS lock held. Public vfs_* functions take the lock; every
// function declared in this header expects the caller to already hold it.
//
// It is a mutex rather than an irq_save() section because disk I/O happens
// underneath it. With IF=0 the timer stops, timer_get_ms() freezes and no
// other thread runs for the length of each sector transfer.
//
// Lock order: kernel_big_lock (if held) -> VFS lock -> driver locks.
// Never call into the DWM or anything that takes kernel_big_lock while
// holding the VFS lock, and never take it from interrupt context.
//

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

// Caller must hold the VFS lock for everything below

uint32_t resolve_path_to_address(const char* path);
uint32_t resolve_parent_path_to_address(const char* path);
uint32_t resolve_path_to_mount_point(const char* path);
bool vfs_parse_path(const char* path, uint16_t flags, uint32_t* out_knode, uint32_t* out_fs_node, bool* out_in_fs, struct FSDeviceContext** out_ctx);
struct FSDeviceContext* vfs_device_get_context(const char* path);
uint8_t* vfs_device_get_block(const char* path);

// The returned descriptor is only valid while the caller keeps holding the
// VFS lock; once it is released another thread may vfs_close() and free it.
OpenFileDescriptor* vfs_file_find_open(File id);

bool vfs_directory_check_mounted(const char* path);

#endif
