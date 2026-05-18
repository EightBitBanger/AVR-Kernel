#include <kernel/string.h>

static inline int is_delimiter(char c, const char *delim) {
    while (*delim) {
        if (c == *delim) {
            return 1;
        }
        delim++;
    }
    return 0;
}

size_t strlen(const char* str) {
    const char *s = str;
    while (*s != '\0') {
        s++;
    }
    return s - str;
}

char* strcpy(char *dest, const char* src) {
    char *ptr = dest;
    while ((*dest++ = *src++) != '\0');
    return ptr;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

char* strtok(char* str, const char* delim) {
    static char *last_token = NULL;
    if (str != NULL) 
        last_token = str;
    
    if (last_token == NULL || *last_token == '\0') 
        return NULL;
    
    while (*last_token && is_delimiter(*last_token, delim)) 
        last_token++;
    
    if (*last_token == '\0') {
        last_token = NULL;
        return NULL;
    }
    
    char* token_start = last_token;
    while (*last_token && !is_delimiter(*last_token, delim)) {
        last_token++;
    }
    
    if (*last_token != '\0') {
        *last_token = '\0'; // Overwrite delimiter with null terminator
        last_token++;       // Move past the null terminator for the next call
    } else {
        last_token = NULL;  // End of string reached
    }
    
    return token_start;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) 
        *p++ = (unsigned char)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) 
        *d++ = *s++;
    return dest;
}

void utos(uint32_t value, char* dest) {
    char buffer[12];
    int8_t i=0;
    int8_t d=0;
    
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    while (i > 0) 
        dest[d++] = buffer[--i];
    
    dest[d] = '\0';
}

void itos(int32_t value, char* dest) {
    char buffer[12]; 
    int8_t i = 0;
    int8_t d = 0;
    uint32_t n;
    
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    if (value < 0) {
        dest[d++] = '-';
        n = (uint32_t)-(int64_t)value; // Cast avoids overflow
    } else {
        n = (uint32_t)value;
    }
    
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    
    while (i > 0) 
        dest[d++] = buffer[--i];
    
    dest[d] = '\0';
}
