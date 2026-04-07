#ifndef SYSCALL_PRINT_H
#define SYSCALL_PRINT_H

#include <stdint.h>

#include <kernel/kernel.h>

void console_init(void);
void console_set_cursor(uint8_t x, uint8_t y);
void console_cursor_blink_rate(uint8_t rate);
void console_clear(void);

uint16_t display_get_width(void);
uint16_t display_get_height(void);

char kb_getc(void);
void print(const char* str);
void print_int(int32_t value);

void print_prompt(void);

void kb_isr_callback(void);
void kb_handler(void);

#endif
