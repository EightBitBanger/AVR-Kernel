#include <avr/io.h>

#include <kernel/kernel.h>

uint8_t PathHome = 'a';

uint8_t PathBin[16];
uint8_t PathBinLength = 0;


uint8_t EnvironmentGetHomeChar(void) {
    
    return PathHome;
}

void EnvironmentSetHomeChar(uint8_t newHomeChar) {
    
    PathHome = newHomeChar;
    
    return;
}


void EnvironmentGetPathBin(uint8_t* path, uint8_t length) {
    
    for (uint8_t i=0; i < length; i++) 
        path[i] = PathBin[i];
    
    return;
}

uint8_t EnvironmentGetPathBinLength(void) {
    
    return PathBinLength;
}

void EnvironmentSetPathBin(uint8_t* path, uint8_t length) {
    
    for (uint8_t i=0; i < length; i++) 
        PathBin[i] = path[i];
    
    PathBinLength = length;
    
    return;
}

