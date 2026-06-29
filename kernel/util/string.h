#include <stdint.h>
#include <stddef.h>

// Strings

size_t strlen(const char* str);
size_t strnlen(const char *str, size_t maxlen);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);

char* strcat(char* dest, const char* src);
size_t strncat(char* dest, const char* src, size_t n);

int strcmp(const char* str1, const char* str2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strtok(char* str, const char* delim);

char* strchr(const char* str, int character);
char* strnchr(const char* str, size_t n, int character);
char* strrchr(const char* str, int character);

char* strstr(const char* haystack, const char* needle);

size_t strspn(const char *str, const char *accept);
size_t strcspn(const char *str, const char *reject);

// Memory

void* memset(void* str, int value, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

// Translation

void itos(int32_t value, char* dest);
void utos(uint32_t value, char* dest);
int32_t stoi(const char* str);
uint32_t stou(const char* str);

// Hex

void u8tox(uint8_t value, char* dest);
void u16tox(uint16_t value, char* dest);
void u32tox(uint32_t value, char* dest);
