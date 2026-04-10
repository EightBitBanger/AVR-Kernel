#ifndef SYSCALL_PRINT_H
#define SYSCALL_PRINT_H

#include <stdint.h>

#include <kernel/kernel.h>

void console_init(void);
void console_clear(void);

void console_set_position(uint8_t x, uint8_t y);
void console_set_prompt(const char* prompt);
void console_set_blink_rate(uint8_t rate);

uint16_t display_get_width(void);
uint16_t display_get_height(void);

char kb_getc(void);
void print(const char* str);
void print_int(int32_t value);
void print_prompt(void);

void console_busy_wait(void);

void kb_isr_callback(void);
void kb_handler(void);

#endif
