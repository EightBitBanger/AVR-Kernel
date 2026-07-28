#ifndef BASE_FILE_H
#define BASE_FILE_H

#include <kernel/fs/fs.h>

typedef struct {
    uint32_t address;
    uint32_t position;
    uint8_t  mode;
    uint8_t  is_open;
    struct FSDeviceContext* ctx;
} FileHandle;

bool fs_file_check(struct FSDeviceContext* ctx, uint32_t address);
uint32_t fs_file_create(struct FSDeviceContext* ctx, const char* name, uint8_t permissions, uint32_t size, uint32_t parent_directory);
bool fs_file_delete(struct FSDeviceContext* ctx, uint32_t address);

bool fs_file_resize(struct FSDeviceContext* ctx, uint32_t address, uint32_t new_size);
uint32_t fs_file_get_size(struct FSDeviceContext* ctx, uint32_t address);
bool fs_file_get_name(struct FSDeviceContext* ctx, uint32_t address, char* filename);
bool fs_file_set_name(struct FSDeviceContext* ctx, uint32_t address, const char* filename);

bool fs_file_get_permissions(struct FSDeviceContext* ctx, uint32_t address, uint8_t* permissions);
bool fs_file_set_permissions(struct FSDeviceContext* ctx, uint32_t address, uint8_t permissions);

bool fs_file_get_attributes(struct FSDeviceContext* ctx, uint32_t address, uint8_t* attributes);
bool fs_file_set_attributes(struct FSDeviceContext* ctx, uint32_t address, uint8_t attributes);

bool fs_file_open(FileHandle* file, struct FSDeviceContext* ctx, uint32_t address, uint8_t mode);
void fs_file_close(FileHandle* file);

uint32_t fs_file_read(FileHandle* file, void* destination, uint32_t size);
uint32_t fs_file_write(FileHandle* file, const void* source, uint32_t size);

bool fs_file_get_certificate(struct FSDeviceContext* ctx, uint32_t address, uint32_t* cert);
bool fs_file_set_certificate(struct FSDeviceContext* ctx, uint32_t address, uint32_t cert);
bool fs_file_get_security_flag(struct FSDeviceContext* ctx, uint32_t address, uint8_t* cert);
bool fs_file_set_security_flag(struct FSDeviceContext* ctx, uint32_t address, uint32_t security);

uint32_t fs_file_seek(FileHandle* file, uint32_t position);
uint32_t fs_file_tell(const FileHandle* file);
uint32_t fs_file_get_address(const FileHandle* file);

#endif
