#ifndef BASE_FILE_H
#define BASE_FILE_H

#include <kernel/fs/fs.h>

#include <stdint.h>

uint32_t fs_file_create(const char* name, uint8_t permissions);
void     fs_file_delete(uint32_t address);

uint32_t fs_directory_create(const char* name, uint8_t permissions);
void     fs_directory_delete(uint32_t address);

uint8_t  fs_directory_add_reference(uint32_t directory_address, uint32_t reference_address);
uint8_t  fs_directory_remove_reference(uint32_t directory_address, uint32_t reference_address);
uint32_t fs_directory_get_reference(uint32_t directory_address, uint32_t index);

#endif
