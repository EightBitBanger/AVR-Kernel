#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>

void print(const char* str);
void print_int(int32_t value);

void print_hex(uint8_t value);
void print_hex16(uint16_t value);
void print_hex32(uint32_t value);

void print_bits(uint8_t value);
void print_bits16(uint16_t value);
void print_bits32(uint32_t value);

#endif
