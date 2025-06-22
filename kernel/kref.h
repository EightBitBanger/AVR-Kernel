#ifndef __KERNEL_REFERENCE_COUNTER_
#define __KERNEL_REFERENCE_COUNTER_

typedef struct {
    
    uint16_t count;
    
} kRef;


// Initialize the reference counter
void kRefInit(kRef* rc);

// Increment the reference counter
void kRefInc(kRef* rc);

// Decrement the reference counter and return the new count
uint8_t kRefDec(kRef* rc);

// Check if the reference counter is zero
uint8_t kRefCheck(kRef* rc);


#endif
