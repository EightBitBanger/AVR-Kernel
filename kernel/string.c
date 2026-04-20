#include <kernel/string.h>
#include <stdint.h>

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
