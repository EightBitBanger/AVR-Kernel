#ifndef _POINTER_UNION__
#define _POINTER_UNION__

union Pointer {
    
    uint32_t address;
    
    uint16_t word_t[2];
    
    uint8_t  byte_t[4];
    
};


#endif
