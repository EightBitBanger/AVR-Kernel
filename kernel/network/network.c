#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/network/network.h>

struct Driver* networkDevice;

void __read_network_device(uint32_t address, uint8_t* buffer);
void __write_network_device(uint32_t address, uint8_t buffer);

uint8_t ntIsInitiated = 0;


uint8_t ntInit(void) {
    
    uint8_t nameNetwork[] = "network";
	networkDevice = (struct Driver*)GetDriverByName( nameNetwork, sizeof(nameNetwork)-1 );
	
	if (networkDevice == nullptr) 
        return 0;
    
	networkDevice->device.bus.read_waitstate  = 10;
	networkDevice->device.bus.write_waitstate = 10;
	
    networkDevice->read  = __read_network_device;
    networkDevice->write = __write_network_device;
    
	// Initiate NIC baud rate
	ntSetBaudRate( NETWORK_BAUD_RATE );
	
	ntIsInitiated = 1;
    
	return 1;
}


void ntSend(uint8_t* buffer, uint8_t bufferSize) {
    
    for (uint8_t i=0; i < bufferSize; i++) {
        
        // Write the buffer
        networkDevice->write( 0x0c, buffer[i]);
        
        // Set the flag to send the byte
        networkDevice->write( 0x0a, 0x01);
        
        uint8_t checkByte=0;
        for (uint16_t a=0; a < 1024; a++) {
            
            networkDevice->read( 0x0a, &checkByte);
            
            if (checkByte == 0) 
                break;
            
            _delay_ms(1);
            
            continue;
        }
        
        continue;
    }
    
    return;
}

uint8_t ntReceive(uint8_t* buffer, uint8_t bufferSize) {
    
    uint8_t receiveBufferSz = 0;
    networkDevice->read( 0x0b, &receiveBufferSz);
    
    for (uint8_t i=0; i < bufferSize; i++) 
        networkDevice->read( 0x10 + i, &buffer[i]);
    
    return receiveBufferSz;
}

void ntReceiveClear(void) {
    
    networkDevice->write( 0x0b, 0x00);
    
    _delay_ms(4);
    
    return;
}

void ntSetBaudRate(uint8_t baudRate) {
    
    networkDevice->write( 0x0e, baudRate);
    networkDevice->write( 0x0d, 0x01);
    _delay_ms(4);
    
    return;
}

uint8_t ntGetBaudRate(void) {
    
    uint8_t baudrate;
    
    networkDevice->read( 0x0e, &baudrate);
    
    return baudrate;
}

void ntBindDevice(uint8_t deviceIndex) {
    
    networkDevice->device.hardware_address = PERIPHERAL_ADDRESS_BEGIN + (deviceIndex * 0x10000);
    
    return;
}

uint8_t ntCheckDevice(void) {
    
    uint8_t buffer[10];
    
    for (uint8_t i=0; i < 10; i++) 
        networkDevice->read( i, &buffer[i]);
    
    uint8_t deviceName[] = "network";
    
    for (uint8_t i=0; i < sizeof(deviceName) - 1; i++) 
        if (deviceName[i] != buffer[i + 1]) 
            return 0;
    
    return 1;
}

uint8_t ntCheckInitiated(void) {
    
    return ntIsInitiated;
}


// Low level IO

void __read_network_device(uint32_t address, uint8_t* buffer) {
    bus_read_byte( &networkDevice->device.bus, networkDevice->device.hardware_address + address, buffer );
    return;
}

void __write_network_device(uint32_t address, uint8_t buffer) {
    bus_write_byte( &networkDevice->device.bus, networkDevice->device.hardware_address + address, buffer );
    return;
}
