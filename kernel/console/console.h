#ifndef CONSOLE_H
#define CONSOLE_H

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


void console_init(char* kb_string, char* kb_prompt);

void console_clear(void);

void console_set_cursor_position(uint8_t position, uint8_t line);

void console_get_path(char* path, uint16_t path_length, uint32_t knode_addr, uint32_t fs_addr, uint8_t depth);


uint16_t console_get_position(void);
uint16_t console_get_line(void);
void console_set_position(uint16_t position);
void console_set_line(uint16_t line);

void console_set_prompt(const char* prompt);
void console_set_blink_rate(uint8_t rate);

uint16_t display_get_width(void);
uint16_t display_get_height(void);
void console_busy_wait(void);

void console_process_command(char* keyboard_str);

void console_set_directory(uint32_t address);
uint32_t console_get_directory(void);

uint32_t console_get_mounted_device(void);
uint32_t console_get_mounted_directory(void);

#endif
