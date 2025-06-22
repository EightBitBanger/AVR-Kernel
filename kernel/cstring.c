#include <kernel/cstring.h>

uint8_t is_number(uint8_t* charPtr) {
    if ((*charPtr >= 0x30) && (*charPtr <= 0x39))
        return 1;
    return 0;
}

uint8_t is_letter(uint8_t* charPtr) {
    if (is_uppercase(charPtr)) return 1;
    if (is_lowercase(charPtr)) return 1;
    return 0;
}

uint8_t is_symbol(uint8_t* charPtr) {
    if ((*charPtr >= 0x21) && (*charPtr <= 0x2f)) return 1;
    if ((*charPtr >= 0x3a) && (*charPtr <= 0x40)) return 1;
    if ((*charPtr >= 0x5b) && (*charPtr <= 0x60)) return 1;
    if ((*charPtr >= 0x7b) && (*charPtr <= 0x7e)) return 1;
    return 0;
}

uint8_t is_hex(uint8_t* charPtr) {
    if (is_letter(charPtr) == 1) 
        lowercase(charPtr);
    uint8_t checksOut = 0;
    if (((*charPtr >= 'a') && (*charPtr <= 'f')) || 
        ((*charPtr >= '0') && (*charPtr <= '9'))) 
        checksOut = 1;
    return checksOut;
}

uint8_t is_uppercase(uint8_t* charPtr) {
    if ((*charPtr >= 0x41) && (*charPtr <= 0x5a))
        return 1;
    return 0;
}

uint8_t is_lowercase(uint8_t* charPtr) {
    if ((*charPtr >= 0x61) && (*charPtr <= 0x7a))
        return 1;
    return 0;
}

void uppercase(uint8_t* charPtr) {
    if (is_lowercase(charPtr) == 1) 
        *charPtr -= 0x20;
    return;
}

void lowercase(uint8_t* charPtr) {
    if (is_uppercase(charPtr) == 1) 
        *charPtr += 0x20;
    return;
}

uint8_t int_to_string(uint32_t number, uint8_t* destination_string) {
    // Special case: “0”
    if (number == 0) {
        destination_string[0] = '0';
        return 1;
    }

    // Temporary buffer to hold digits (in reverse order).
    // Max 10 digits for a 32-bit number (0 .. 4,294,967,295).
    uint8_t revbuf[10];
    uint8_t len = 0;

    // Extract digits one at a time (least significant first).
    while (number > 0) {
        uint8_t digit = number % 10;     // 0–9
        revbuf[len++] = '0' + digit;     // store ASCII('0' + digit)
        number /= 10;
    }
    
    // Now revbuf[0..len-1] holds digits in reverse. 
    // Copy them back in correct order:
    for (uint8_t i = 0; i < len; i++) {
        destination_string[i] = revbuf[len - 1 - i];
    }
    
    return len;
}

void int_to_hex_string(uint32_t integer, uint8_t* string) {
    for (int i = 0; i < 8; i++) {
        // Grab the topmost nibble of what’s left:
        uint8_t nibble = (integer >> 28) & 0x0F;
        
        // Convert 0–15 → '0'–'9' or 'a'–'f'
        if (nibble < 10) {
            string[i] = '0' + nibble;
        } else {
            string[i] = 'a' + (nibble - 10);
        }
        
        // Shift off the nibble we just processed
        integer <<= 4;
    }
    return;
}

uint8_t string_get_int(uint8_t* string) {
    uint8_t integer = 0;
    
    // Check shift up
    if ((string[1] == ' ') && (string[2] == ' ')) {
        string[2] = string[0];
        string[1] = ' ';
        string[0] = ' ';
    }
    
    if (string[2] == ' ') {
        string[2] = string[1];
        string[1] = string[0];
        string[0] = ' ';
    }
    
    if (is_number(&string[2])) integer += (string[2] - '0') * 1;
    if (is_number(&string[1])) integer += (string[1] - '0') * 10;
    if (is_number(&string[0])) integer += (string[0] - '0') * 100;
    
    return integer;
}

uint32_t string_get_int_long(uint8_t* string) {
    uint32_t integer = 0;
    
    if (is_number(&string[3])) { integer += (string[3] - '0') * 1; }
    if (is_number(&string[2])) { integer += (string[2] - '0') * 10; }
    if (is_number(&string[1])) { integer += (string[1] - '0') * 100; }
    if (is_number(&string[0])) { integer += (string[0] - '0') * 1000; }
    return integer;
}

uint8_t string_get_hex_char(uint8_t* string) {
    uint32_t value_a = 0;
    uint32_t value_b = 0;
    uint8_t hex;
    
    // First digit
    hex = string[1];
    if ((hex >= 0x30) && (hex <= 0x40)) value_b += hex - 0x30;
    if ((hex >= 0x60) && (hex <= 0x67)) value_b += 9 + (hex - 0x60);
    
    // Second digit
    hex = string[0];
    if ((hex >= 0x30) && (hex <= 0x40)) value_a += hex - 0x30;
    if ((hex >= 0x60) && (hex <= 0x67)) value_a += 9 + (hex - 0x60);
    
    return value_a + (value_b * 16);
}

uint8_t StringCompare(uint8_t* stringA, uint8_t lengthA, uint8_t* stringB, uint8_t lengthB) {
    uint8_t strA[10];
    uint8_t strB[10];
    
    for (uint8_t i = 0; i < 10; i++) {
        strA[i] = ' ';
        strB[i] = ' ';
    }
    
    for (uint8_t i = 0; i < lengthA; i++) strA[i] = stringA[i];
    for (uint8_t i = 0; i < lengthB; i++) strB[i] = stringB[i];
    
    for (uint8_t i = 0; i < 10; i++) {
        if (strA[i] != strB[i]) return 0;
    }
    
    return 1;
}

uint8_t StringSplit(uint8_t* source, uint8_t sourceLen, 
                    uint8_t* splitA, uint8_t* splitALen, 
                    uint8_t* splitB, uint8_t* splitBLen, 
                    uint8_t delimiter) {
    uint8_t split = 0;
    uint8_t index = 0;
    
    for (uint8_t i = 0; i < sourceLen; i++) {
        if (split == 0) {
            splitA[i] = source[i];
            if (source[i] == delimiter) {
                split = 1;
                *splitALen = i + 1;
                continue;
            }
        } else {
            splitB[index] = source[i];
            *splitBLen = index + 2;
            if (source[i] == delimiter) break;
            index++;
        }
    }
    
    return 1;
}

uint8_t StringFindChar(uint8_t* string, uint8_t length, uint8_t character) {
    for (uint8_t i = 0; i < length; i++) 
        if (string[i] == character) 
            return i;
    
    return 1;
}


uint8_t BitSet(uint8_t byte, uint8_t index, uint8_t value) {
    if (value) {
        byte |= (1 << index);  // 1
    } else {
        byte &= ~(1 << index);  // 0
    }
    return byte;
}

uint8_t bit_set(uint8_t byte, uint8_t index) {
    byte |= (1 << index);
    return byte;
}

uint8_t bit_clear(uint8_t byte, uint8_t index) {
    byte &= ~(1 << index);
    return byte;
}

uint8_t bit_get(uint8_t byte, uint8_t bit_index) {
    return (byte >> bit_index) & 1;
}


