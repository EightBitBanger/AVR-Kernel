#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#define KEYBOARD_STRING_LENGTH  32

struct WorkingDirectory {
    uint32_t current_directory;
    uint32_t mount_device;
    uint32_t mount_directory;
    uint32_t mount_root;
};

struct LocalPaths {
    char path[64];
};


void console_init(char* kb_string, char* kb_prompt, uint8_t kb_string_max_length, uint8_t kb_prompt_max_length);

void console_process_command(char* keyboard_str);

void console_get_path(char* path, uint16_t path_length, uint32_t knode_addr, uint32_t fs_addr, uint8_t depth);

void console_prompt_print(void);
void console_prompt_set_string(const char* prompt);

uint8_t console_get_keyboard_string_length(void);
void console_get_keyboard_string(char* kb_string);

void console_set_directory(uint32_t address);
uint32_t console_get_directory(void);

uint32_t console_get_mounted_device(void);
uint32_t console_get_mounted_directory(void);

void console_print_fs_entry(uint32_t directory_address);
void console_print_reference_entry(uint32_t address);

#endif
