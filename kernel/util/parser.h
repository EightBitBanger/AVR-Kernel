#ifndef _C_STRING_PARSER_H_
#define _C_STRING_PARSER_H_

#include <stdint.h>
#include <stddef.h>

// Remove any spaces at the beginning of a string (in-place)
void parse_trim_leading_spaces(char* str);

// Get the filename part of a path
void parse_get_filename(const char* path, char* buffer, uint16_t buffer_size);

// Get the parent path leading up to the current path
void parse_get_parent_path(const char* path, char* buffer, uint16_t buffer_size);

// String functions

// Convert a string to lowercase (in place)
void str_tolower(char *str);

// Convert a string to uppercase (in place)
void str_toupper(char *str);

// Return a string with 'to_replace' replaced with 'to_find'
int str_replace(char *dest, const char *to_find, const char *to_replace, int max_size);

#endif
