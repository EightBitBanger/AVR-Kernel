#include <kernel/drivers.h>

void InitBakedDrivers(void) {
    
    initiateDisplayDriver();      // Display
	initiatePS2Driver();          // PS/2 Port
	
    return;
}
