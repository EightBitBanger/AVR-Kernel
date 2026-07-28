#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static inline void append_char(char *str, size_t size, size_t *written, char c) {
    if (*written + 1 < size) {
        str[*written] = c;
    }
    (*written)++;
}

static void format_number(char *str, size_t size, size_t *written, uint64_t num, int base, int is_signed, int uppercase) {
    char buf[65];
    size_t i = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    if (is_signed && (int64_t)num < 0) {
        append_char(str, size, written, '-');
        num = (uint64_t)(-(int64_t)num);
    }
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = digits[num % base];
            num /= base;
        }
    }
    
    while (i > 0) {
        append_char(str, size, written, buf[--i]);
    }
}

int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    size_t written = 0;
    
    for (const char *p = format; *p != '\0'; p++) {
        if (*p != '%') {
            append_char(str, size, &written, *p);
            continue;
        }
        
        p++; // Skip '%'
        
        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                append_char(str, size, &written, c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) {
                    append_char(str, size, &written, *s++);
                }
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                format_number(str, size, &written, (uint64_t)val, 10, 1, 0);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                format_number(str, size, &written, val, 10, 0, 0);
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                format_number(str, size, &written, val, 16, 0, 0);
                break;
            }
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                format_number(str, size, &written, val, 16, 0, 1);
                break;
            }
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(args, void *);
                append_char(str, size, &written, '0');
                append_char(str, size, &written, 'x');
                format_number(str, size, &written, ptr, 16, 0, 0);
                break;
            }
            case '%': {
                append_char(str, size, &written, '%');
                break;
            }
            default: {
                // Unknown specifier: output literally
                append_char(str, size, &written, '%');
                if (*p != '\0') {
                    append_char(str, size, &written, *p);
                } else {
                    p--; // Handle trailing '%' at end of string
                }
                break;
            }
        }
    }
    
    // Always null-terminate if size > 0
    if (size > 0) {
        if (written < size) {
            str[written] = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    
    return (int)written;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}
