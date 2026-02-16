#ifndef __SYSCALL_GRAPHICS_API_
#define __SYSCALL_GRAPHICS_API_

#include <kernel/kernel.h>

#define GL_MODE_TEXT       0
#define GL_MODE_SPRITES    2
#define GL_MODE_GRAPHICS   4

#define GL_TRIANGLES  0
#define GL_LINES      1

typedef struct { int8_t x, y; } Point2;

typedef struct {
    Point2 x;
    Point2 y;
    Point2 z;
    
    int8_t dither;
} Vertex;

union FloatBytes {
    float value;
    uint8_t bytes[4];
};


void swapBuffers(void);

int8_t glInit(uint8_t mode);
void glClear(uint8_t character);

void glBegin(uint8_t primitive);

void glBufferSetSize(uint8_t size);

void glUniformTranslate(float x, float y, float z);
void glUniformRotation(float yaw, float pitch, float roll);
void glUniformScale(float scale);

void glBufferData(int8_t* buffer, uint16_t size);

void glDrawBuffer(uint8_t begin, uint8_t size);


void __glBusyWait(void);

#endif
