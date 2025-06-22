#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>
#include <kernel/syscalls/timer/timer.h>

#include <kernel/command/graph/graph.h>

int8_t vertexBuffer[] = {
    -10, -10, -10,   10, -10, -10,   10,  10, -10,  3, // Front face
     10,  10, -10,  -10,  10, -10,  -10, -10, -10,  3, 
     10, -10, -10,   10, -10,  10,   10,  10,  10,  2, // Right face
     10,  10,  10,   10,  10, -10,   10, -10, -10,  2, 
     10, -10,  10,  -10, -10,  10,  -10,  10,  10,  3, // Back face
    -10,  10,  10,   10,  10,  10,   10, -10,  10,  3, 
    -10, -10,  10,  -10, -10, -10,  -10,  10, -10,  2, // Left face
    -10,  10, -10,  -10,  10,  10,  -10, -10,  10,  2, 
    -10,  10, -10,   10,  10, -10,   10,  10,  10,  2, // Top face
     10,  10,  10,  -10,  10,  10,  -10,  10, -10,  2, 
    -10, -10, -10,  -10, -10,  10,   10, -10,  10,  2, // Bottom face
     10, -10,  10,   10, -10, -10,  -10, -10, -10,  2 , 
};


void functionGRAPH(uint8_t* param, uint8_t param_length) {
    glInit(GL_MODE_GRAPHICS);
    glClear(0);
    
    glBufferData(vertexBuffer, sizeof(vertexBuffer));
    
    // Fire up the timer
    EnableGlobalInterrupts();
    
    glUniformRotation(0.0f, 0.0f, 0.0f);
    //glUniformScale(100.0f);
    
    float rotationX=0.0f;
    float rotationY=0.0f;
    float rotationZ=0.0f;
    
    glUniformScale(9.0f);
    
    //uint64_t lastTime = time_ms();
    
    while(1) {
        
        //uint64_t currentTime = time_ms();
        //uint64_t delta = currentTime - lastTime;
        
        //if (delta < 4) 
        //    continue;
        //lastTime = currentTime;
        
        // Update
        rotationX += 1.0f;
        rotationY += 1.4f;
        rotationZ += 1.8f;
        
        // Render
        glBegin(GL_LINES);
        float posOffset = 0.0f;//120.0f;
        
        glUniformRotation(rotationX, rotationY, rotationZ);
        glUniformTranslate(-posOffset, 0.0f, 0.0f);
        glDrawBuffer(0, sizeof(vertexBuffer) / 10);
        
        //glUniformRotation(-rotationX, -rotationY, -rotationZ);
        //glUniformTranslate(posOffset, 0.0f, 0.0f);
        //glDrawBuffer(0, sizeof(vertexBuffer) / 10);
        
        swapBuffers();
        glClear(0);
    }
    
    glInit(0);
    
    return;
}


void registerCommandGRAPHICS(void) {
    
    uint8_t graphCommandName[] = "graph";
    
    ConsoleRegisterCommand(graphCommandName, sizeof(graphCommandName), functionGRAPH);
    
    return;
}

