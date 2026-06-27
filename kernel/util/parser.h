#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>

void parse_trim_leading_spaces(char *str);

void parse_get_filename(const char* path, char* buffer, uint16_t buffer_size);

#endif
