#ifndef BASE_FILE_H
#define BASE_FILE_H

#include <kernel/fs/fs.h>

typedef struct {
    uint32_t address;
    uint32_t position;
    uint8_t  mode;
    uint8_t  is_open;
} FileHandle;

uint32_t fs_file_create(const char* name, uint8_t permissions, uint32_t size, uint32_t parent_directory);
void fs_file_delete(uint32_t address);

uint32_t fs_file_get_size(uint32_t address);
void fs_file_get_name(uint32_t address, char* filename);
void fs_file_set_name(uint32_t address, char* filename);

bool fs_file_get_permissions(uint32_t address, uint8_t* permissions);
bool fs_file_set_permissions(uint32_t address, uint8_t permissions);

bool fs_file_get_attributes(uint32_t address, uint8_t* attributes);
bool fs_file_set_attributes(uint32_t address, uint8_t attributes);

bool fs_file_open(FileHandle* file, uint32_t address, uint8_t mode);
void fs_file_close(FileHandle* file);

uint32_t fs_file_read(FileHandle* file, void* destination, uint32_t size);
uint32_t fs_file_write(FileHandle* file, const void* source, uint32_t size);

uint32_t fs_file_seek(FileHandle* file, uint32_t position);
uint32_t fs_file_tell(const FileHandle* file);
uint32_t fs_file_get_address(const FileHandle* file);

#endif
