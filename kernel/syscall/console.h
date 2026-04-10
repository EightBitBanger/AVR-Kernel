#ifndef CONSOLE_H
#define CONSOLE_H

void process_command(char* keyboard_str);

void console_set_directory(uint32_t address);
uint32_t console_get_directory(void);

#endif
