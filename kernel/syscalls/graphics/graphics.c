#include <kernel/syscalls/graphics/graphics.h>

extern struct Driver* displayDevice;


union Resigner {
    uint8_t unsign;
    int8_t sign;
};


void swapBuffers(void) {
    __glBusyWait();
    displayDevice->write(0x0000F, 1); // Trigger a buffer swap
    return;
}

int8_t glInit(uint8_t mode) {
    __glBusyWait();
    displayDevice->write(0x00000, mode);
    return 1;
}

void glBufferSetSize(uint8_t size) {
    __glBusyWait();
    displayDevice->write(0x00008, size);
    return;
}

void glUniformTranslate(float x, float y, float z) {
    __glBusyWait();
    uint8_t offset = 0; // Byte offset to the uniform
    
    union FloatBytes translateX;
    union FloatBytes translateY;
    union FloatBytes translateZ;
    translateX.value = x;
    translateY.value = y;
    translateZ.value = z;
    
    displayDevice->write(0x00007, 4); // Access uniform values
    for (uint8_t i=0; i < 4; i++) {
        displayDevice->write(0x000010 + i + offset, translateX.bytes[i]);
        displayDevice->write(0x000010 + i + 4 + offset, translateY.bytes[i]);
        displayDevice->write(0x000010 + i + 8 + offset, translateZ.bytes[i]);
    }
    
    displayDevice->write(0x00007, 0); // Reset to VRAM
    return;
}

void glUniformRotation(float yaw, float pitch, float roll) {
    __glBusyWait();
    uint8_t offset = 12; // Byte offset to the uniform
    
    union FloatBytes rotationX;
    union FloatBytes rotationY;
    union FloatBytes rotationZ;
    
    rotationX.value = yaw;
    rotationY.value = pitch;
    rotationZ.value = roll;
    
    displayDevice->write(0x00007, 4); // Access uniform values
    
    for (uint8_t i=0; i < 4; i++) {
        displayDevice->write(0x000010 + i + offset, rotationX.bytes[i]);
        displayDevice->write(0x000010 + i + 4 + offset, rotationY.bytes[i]);
        displayDevice->write(0x000010 + i + 8 + offset, rotationZ.bytes[i]);
    }
    
    displayDevice->write(0x00007, 0); // Reset to VRAM
    return;
}

void glUniformScale(float scale) {
    __glBusyWait();
    uint8_t offset = 24; // Byte offset to the uniform
    
    union FloatBytes uniformScale;
    uniformScale.value = scale;
    
    displayDevice->write(0x00007, 4); // Access uniform values
    
    for (uint8_t i=0; i < 4; i++) {
        displayDevice->write(0x000010 + i + offset, uniformScale.bytes[i]);
    }
    
    displayDevice->write(0x00007, 0); // Reset to VRAM
    return;
}

void glBegin(uint8_t primitive) {
    __glBusyWait();
    displayDevice->write(0x0000A, primitive); // Set primitive drawing type
    return;
}

void glBufferData(int8_t* buffer, uint16_t size) {
    __glBusyWait();
    displayDevice->write(0x00001, 0);
    displayDevice->write(0x00002, 0);
    displayDevice->write(0x00007, 3); // Access vertex buffer
    
    for (uint16_t i=0; i < size; i++) {
        union Resigner signswap;
        signswap.sign = buffer[i];
        displayDevice->write(0x00010 + i, signswap.unsign);
    }
    
    displayDevice->write(0x00007, 0); // Back to VRAM
    displayDevice->write(0x00008, size / 10); // Set the buffer size
    return;
}

void glDrawBuffer(uint8_t begin, uint8_t size) {
    __glBusyWait();
    displayDevice->write(0x0000C, begin); // Starting offset
    displayDevice->write(0x0000D, size);  // Number of bytes
    displayDevice->write(0x0000E, 1);     // Trigger the draw
    __glBusyWait();
    return;
}

void glClear(uint8_t character) {
    __glBusyWait();
    displayDevice->write(0x00009, 1);
    return;
}


void __glBusyWait(void) {
    DeviceBusyWait((struct Device*)displayDevice, 0x10);
    return;
};
