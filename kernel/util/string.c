#include <stdint.h>
#include <stddef.h>

static inline int is_delimiter(char c, const char *delim) {
    while (*delim) {
        if (c == *delim) {
            return 1;
        }
        delim++;
    }
    return 0;
}

// String operations

size_t strlen(const char* str) {
    const char *s = str;
    while (*s != '\0') {
        s++;
    }
    return (size_t)(s - str);
}

char* strcpy(char* dest, const char* src) {
    char *ptr = dest;
    while ((*dest++ = *src++) != '\0');
    return ptr;
}

size_t strnlen(const char *str, size_t maxlen) {
    const char *s = str;
    while (maxlen > 0 && *s != '\0') {
        s++;
        maxlen--;
    }
    return (size_t)(s - str);
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    // Pad out the remainder of the buffer with null bytes
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char *ptr = dest;
    
    // Find the end of dest
    while (*ptr != '\0') {
        ptr++;
    }
    
    // Copy src to the end of dest
    while ((*ptr++ = *src++) != '\0');
    
    return dest;
}

size_t strncat(char* dest, const char* src, size_t size) {
    char *d = dest;
    const char *s = src;
    size_t dlen;
    size_t n = size;
    
    // Find the end of dest and determine its length
    while (n-- != 0 && *d != '\0') {
        d++;
    }
    dlen = d - dest;
    n = size - dlen;
    
    if (n == 0) {
        return dlen + strlen(s);
    }
    
    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';
    
    return dlen + (s - src);
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) 
        return 0;
    
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) 
        return 0;
    
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char* strtok(char* str, const char* delim) {
    static char *last_token = NULL;
    
    if (str != NULL) {
        last_token = str;
    }
    
    if (last_token == NULL || *last_token == '\0') {
        return NULL;
    }
    
    // Skip leading delimiters
    while (*last_token && is_delimiter(*last_token, delim)) {
        last_token++;
    }
    
    if (*last_token == '\0') {
        last_token = NULL;
        return NULL;
    }
    
    char* token_start = last_token;
    
    // Find the end of the token
    while (*last_token && !is_delimiter(*last_token, delim)) {
        last_token++;
    }
    
    if (*last_token != '\0') {
        *last_token = '\0'; 
        last_token++;       
    } else {
        last_token = NULL;  
    }
    
    return token_start;
}

char* strchr(const char* str, int character) {
    while (*str != (char)character) {
        if (!*str) {
            return NULL;
        }
        str++;
    }
    return (char*)str;
}

char* strnchr(const char* str, size_t n, int character) {
    while (n > 0) {
        if (*str == (char)character) {
            return (char*)str;
        }
        if (*str == '\0') {
            return NULL;
        }
        str++;
        n--;
    }
    return NULL;
}

char* strrchr(const char* str, int character) {
    const char *last = NULL;
    char c = (char)character;
    
    while (*str != '\0') {
        if (*str == c) {
            last = str;
        }
        str++;
    }
    
    // Standard behavior: if searching for '\0', return pointer to the terminator
    if (c == '\0') {
        return (char*)str;
    }
    
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) {
        return (char*)haystack;
    }
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) {
                return (char*)haystack;
            }
        }
    }
    return NULL;
}

size_t strspn(const char* str, const char *accept) {
    const char *s = str;
    while (*s != '\0') {
        if (!is_delimiter(*s, accept)) {
            break;
        }
        s++;
    }
    return (size_t)(s - str);
}

size_t strcspn(const char* str, const char *reject) {
    const char *s = str;
    while (*s != '\0') {
        if (is_delimiter(*s, reject)) {
            break;
        }
        s++;
    }
    return (size_t)(s - str);
}

// Memory Operations

void* memset(void* str, int value, size_t n) {
    unsigned char* ptr = (unsigned char*)str;
    while (n--) {
        *ptr++ = (unsigned char)value;
    }
    return str;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        // Copy forward
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        // Copy backward to avoid overwriting src data
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// Translation Operations

void utos(uint32_t value, char* dest) {
    char buffer[12]; // 10 digits max for uint32_t + null terminator + safe padding
    int32_t i = 0;
    int32_t d = 0;
    
    // Explicitly handle 0 case
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    while (value > 0) {
        buffer[i++] = (char)((value % 10) + '0');
        value /= 10;
    }
    
    // Reverse the string into destination
    while (i > 0) {
        dest[d++] = buffer[--i];
    }
    
    dest[d] = '\0';
}

void itos(int32_t value, char* dest) {
    char buffer[12]; 
    int32_t i = 0;
    int32_t d = 0;
    uint32_t n;
    
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    if (value < 0) {
        dest[d++] = '-';
        // Handle INT32_MIN overflow safely by casting up before negating
        n = (uint32_t)(-(int64_t)value); 
    } else {
        n = (uint32_t)value;
    }
    
    while (n > 0) {
        buffer[i++] = (char)((n % 10) + '0');
        n /= 10;
    }
    
    while (i > 0) {
        dest[d++] = buffer[--i];
    }
    
    dest[d] = '\0';
}

int32_t stoi(const char* str) {
    int32_t result = 0;
    int32_t sign = 1;
    
    // Skip whitespace
    while (*str == ' ' || (*str >= '\t' && *str <= '\r')) {
        str++;
    }
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

uint32_t stou(const char* str) {
    uint32_t result = 0;
    
    // Skip whitespace characters
    while (*str == ' ' || (*str >= '\t' && *str <= '\r')) {
        str++;
    }
    
    // Optional leading plus sign is valid for unsigned parsing
    if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result;
}

void u8tox(uint8_t value, char* dest) {
    char buffer[3]; // 2 digits max for uint8_t + null terminator
    int32_t i = 0;
    int32_t d = 0;
    const char hex_digits[] = "0123456789ABCDEF";
    
    // Explicitly handle 0 case
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    while (value > 0) {
        buffer[i++] = hex_digits[value % 16];
        value /= 16;
    }
    
    // Reverse the string into destination
    while (i > 0) {
        dest[d++] = buffer[--i];
    }
    
    dest[d] = '\0';
}

void u16tox(uint16_t value, char* dest) {
    char buffer[5]; // 4 digits max for uint16_t + null terminator
    int32_t i = 0;
    int32_t d = 0;
    const char hex_digits[] = "0123456789ABCDEF";
    
    // Explicitly handle 0 case
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    while (value > 0) {
        buffer[i++] = hex_digits[value % 16];
        value /= 16;
    }
    
    // Reverse the string into destination
    while (i > 0) {
        dest[d++] = buffer[--i];
    }
    
    dest[d] = '\0';
}

void u32tox(uint32_t value, char* dest) {
    char buffer[9]; // 8 digits max for uint32_t + null terminator
    int32_t i = 0;
    int32_t d = 0;
    const char hex_digits[] = "0123456789ABCDEF";
    
    // Explicitly handle 0 case
    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return;
    }
    
    while (value > 0) {
        buffer[i++] = hex_digits[value % 16];
        value /= 16;
    }
    
    // Reverse the string into destination
    while (i > 0) {
        dest[d++] = buffer[--i];
    }
    
    dest[d] = '\0';
}
