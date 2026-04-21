#ifndef SYSCALL_PRINT_H
#define SYSCALL_PRINT_H

#include <stdint.h>

#include <kernel/kernel.h>

void kb_init(void);

void print(const char* str);
void print_int(int32_t value);

void print_hex(uint8_t value);
void print_hex16(uint16_t value);
void print_hex32(uint32_t value);

void print_bits(uint8_t value);
void print_bits16(uint16_t value);
void print_bits32(uint32_t value);

void print_prompt(void);

char kb_getc(void);

uint8_t kb_check_input_state(void);
uint8_t kb_get_current_char(void);
void kb_clear_input_state(void);

void kb_isr_callback(void);
void kb_event_handler(void);

#endif
