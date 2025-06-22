#include <drivers/display/LiquidCrystalDisplayController/main.h>

#include <kernel/kernel.h>

struct DisplayDeviceDriver displayDeviceDriver;

struct DisplayDeviceDriver* displayDriver = &displayDeviceDriver;


// Driver function declarations

void __read_display_device(uint32_t address, uint8_t* buffer);
void __write_display_device(uint32_t address, uint8_t buffer);


void initiateDisplayDriver(void) {
    
    uint8_t deviceName[] = "display";
    
    for (unsigned int i=0; i < sizeof(deviceName); i++) 
        displayDriver->device.device_name[i] = deviceName[i];
    
    // Register the driver with the kernel
	RegisterDriver( (void*)displayDriver );
	
	// Set hardware device details
	displayDriver->device.hardware_address = PERIPHERAL_ADDRESS_BEGIN;
	
	displayDriver->device.device_id = 0x10;
	
	displayDriver->device.bus.read_waitstate  = 8;
	displayDriver->device.bus.write_waitstate = 8;
    
    displayDriver->read  = __read_display_device;
    displayDriver->write = __write_display_device;
    
    displayDriver->is_linked = 0;
    
    return;
}


void __read_display_device(uint32_t address, uint8_t* buffer) {
    bus_read_byte( &displayDriver->device.bus, displayDriver->device.hardware_address + address, buffer );
    return;
}

void __write_display_device(uint32_t address, uint8_t buffer) {
    bus_write_byte( &displayDriver->device.bus, displayDriver->device.hardware_address + address, buffer );
    return;
}
