#include <kernel/console/virtual_key.h>
#include <string.h>

volatile uint8_t* VirtualKeyMap = (void*)0;

void kb_map_init(uint8_t* map_ptr, uint16_t size) {
    memset(map_ptr, 0x00, size);
    
    VirtualKeyMap = map_ptr;
    
    for (uint8_t i=0; i < sizeof(VirtualKeyMap); i++) 
        VirtualKeyMap[i] = 0x00;
}

char kb_vkey_check(uint8_t key) {
    if (key < sizeof(VirtualKeyMap)) 
        return VirtualKeyMap[key];
    return 0;
}

char kb_vkey_set(uint8_t key, uint8_t state) {
    if (key < sizeof(VirtualKeyMap)) 
        VirtualKeyMap[key] = state;
}
