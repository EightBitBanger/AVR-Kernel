#include <drivers/network/NIC/main.h>

#include <kernel/kernel.h>
/*
struct NetworkDeviceDriver networkDeviceDriver;

struct NetworkDeviceDriver* networkDriver = &networkDeviceDriver;


// Driver function declarations

void __read_network_device(uint32_t address, uint8_t* buffer);
void __write_network_device(uint32_t address, uint8_t buffer);


void initiateNetworkDriver(void) {
    
    uint8_t deviceName[] = "network";
    
    for (unsigned int i=0; i < sizeof(deviceName); i++) 
        networkDriver->device.device_name[i] = deviceName[i];
    
    // Register the driver with the kernel
	RegisterDriver( (void*)networkDriver );
	
	// Set hardware device details
	networkDriver->device.hardware_address = 0x00000;
	
	networkDriver->device.device_id = 0x14;
	
	networkDriver->interface.read_waitstate  = 20;
	networkDriver->interface.write_waitstate = 20;
    
    networkDriver->read  = __read_network_device;
    networkDriver->write = __write_network_device;
    
    networkDriver->is_linked = 0;
    
    return;
}


void __read_network_device(uint32_t address, uint8_t* buffer) {
    bus_read_byte( &networkDriver->interface, networkDriver->device.hardware_address + address, buffer );
    return;
}

void __write_network_device(uint32_t address, uint8_t buffer) {
    bus_write_byte( &networkDriver->interface, networkDriver->device.hardware_address + address, buffer );
    return;
}
*/
