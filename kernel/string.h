#include <stdint.h>
#include <stddef.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
char* strtok(char* str, const char* delim);

void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);

void itos(int32_t value, char* dest);
void utos(uint32_t value, char* dest);
