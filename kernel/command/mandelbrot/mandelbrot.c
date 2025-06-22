#include <avr/io.h>

#include <kernel/kernel.h>

#include <kernel/command/mandelbrot/mandelbrot.h>

#define WIDTH     24
#define HEIGHT    10
#define MAX_ITER  100

extern struct Driver* displayDevice;

int mandelbrot(double real, double imag) {
    double r = 0.0, i = 0.0;
    int iter = 0;
    
    while (r * r + i * i <= 4.0 && iter < MAX_ITER) {
        double r_temp = r * r - i * i + real;
        i = 2 * r * i + imag;
        r = r_temp;
        iter++;
    }
    
    return iter;
}




uint8_t buffer[8] = {0b01000000,
                     0b00101000,
                     0b00010100,
                     0b11111110,
                     0b00010100,
                     0b00101000,
                     0b01000000,
                     0b00000000};


uint8_t worldMap[8 * 16] = { 'T',' ','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             'T','T','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             ' ',' ','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             'T',' ',' ','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             'T','T',' ','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             ' ','T','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             'T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', 
                             };







void functionMandelbrot(uint8_t* param, uint8_t param_length) {
    
    displayDevice->write(0x00000, 1);
    
    displayDevice->write(0x00001, 0);
    displayDevice->write(0x00002, 0);
    
    // Clear the display
    for (uint32_t r=0; r < 8; r++) {
        
        displayDevice->write(0x00001, r);
        
        for (uint32_t c=0; c < 128; c++) 
            displayDevice->write( 0x00010 + c, 0x00 );
        
    }
    
    displayDevice->write(0x00001, 0);
    displayDevice->write(0x00002, 0);
    
    for (uint8_t l=0; l < 8; l++) {
        
        for (uint8_t w=0; w < 16; w++) {
            
            if (worldMap[w + (l * 16)] == 'T') {
                
                displayDevice->write(0x00001, l);
                displayDevice->write(0x00002, w * 8);
                
                for (uint32_t c=0; c < 8; c++) 
                    displayDevice->write( 0x00010 + c, buffer[c] );
                
            }
            
        }
        
    }
    
    
    
    
    
    
    
    while(1) {}
    
    
    /*
    
    //for (uint32_t l=0; l < 8; l++) {
        
        displayDevice->write(0x00001, 0);
        
        for (uint32_t c=0; c < 128; c++) {
            
            value += (c * 32) / (c + 1);
            
            displayDevice->write(0x00010 + c, value);
            
        }
        
    //}
    
    */
    
    
    
    
    return;
    
    /*
    
    for (int y = 0; y < HEIGHT; y++) {
        
        for (int x = 0; x < WIDTH; x++) {
            
            double real = (x - WIDTH / 2.0) * 4.0 / WIDTH;
            double imag = (y - HEIGHT / 2.0) * 4.0 / HEIGHT;
            
            int iter = mandelbrot(real, imag);
            
            if (iter == MAX_ITER) 
            
            
            printChar(c);
        }
        
        printLn();
    }
    */
    
    return;
}

void registerCommandMandelbrot(void) {
    
    uint8_t mandelbrotCommandName[] = "brot";
    
    ConsoleRegisterCommand(mandelbrotCommandName, sizeof(mandelbrotCommandName), functionMandelbrot);
    
    return;
}
