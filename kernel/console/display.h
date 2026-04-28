#ifndef DISPLAY_IO_H
#define DISPLAY_IO_H

void display_init(void);

uint16_t display_get_width(void);
uint16_t display_get_height(void);

void display_putc(const char ch);
void display_newline(void);

void display_cursor_set_position(uint16_t position);
void display_cursor_set_line(uint16_t line);
uint16_t display_cursor_get_position(void);
uint16_t display_cursor_get_line(void);

void display_clear(void);

void display_busy_wait(void);
uint32_t display_get_device_address(void);

#endif
