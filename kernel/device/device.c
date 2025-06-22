#include <stdint.h>

#include <kernel/delay.h>
#include <kernel/cstring.h>

#include <kernel/device/device.h>
#include <kernel/driver/driver.h>

struct Node* DeviceTableHead = NULL;


struct BusDescriptor {
    
    struct Node* deviceBusNode;
    
};

struct Bus deviceBus;



void InitiateDeviceTable(void) {
    
    deviceBus.read_waitstate  = 20;
    deviceBus.write_waitstate = 20;
    
    //
    // Initiate peripheral devices
    
    unsigned int index=0;
    
    for (uint32_t d=0; d < NUMBER_OF_PERIPHERALS; d++) {
        
        // Allow time between device initiation
        _delay_us(100);
        
        // Calculate device address offset
        uint32_t hardware_address = PERIPHERAL_ADDRESS_BEGIN + (PERIPHERAL_STRIDE * d);
        
        // Get device ID
        uint8_t deviceID;
        bus_read_byte(&deviceBus, hardware_address, &deviceID);
        
        if (deviceID == 0xff) 
            continue;
        
        struct Device* newDevicePtr = (struct Device*)malloc(sizeof(struct Device));
        ListAddNode(&DeviceTableHead, newDevicePtr);
        
        // Set device ID byte
        newDevicePtr->device_id = deviceID;
        
        // Get device name
        for (uint8_t i=0; i < DEVICE_NAME_LEN; i++) 
            bus_read_byte(&deviceBus, hardware_address + i + 1, &newDevicePtr->device_name[i]);
        
        // Blank the name once a space character is found
        uint8_t checkFinished = 0;
        for (uint8_t i=0; i < DEVICE_NAME_LEN; i++) {
            
            if (newDevicePtr->device_name[i] == ' ') 
                checkFinished = 1;
            
            if (checkFinished != 0) 
                newDevicePtr->device_name[i] = ' ';
        }
        
        newDevicePtr->hardware_address = hardware_address;
        newDevicePtr->hardware_slot = d;
        index++;
        
        continue;
    }
    
    
    //
    // Link the hardware to their associated device drivers
    
    // Check the registered drivers
    for (unsigned int i=0; i < GetNumberOfDrivers(); i++) {
        
        uint32_t numberOfDevices = ListGetSize(DeviceTableHead);
        
        // Look up the device to link the driver
        for (unsigned int d=0; d < numberOfDevices; d++) {
            
            struct Node* node = ListGetNode(DeviceTableHead, d);
            struct Device* devicePtr = (struct Device*)node->data;
            
            // Get the device from the driver
            struct Driver* driver = GetDriverByIndex(i);
            struct Device* device = (struct Device*)GetDriverByIndex(i);
            
            // Check hardware validation
            if (device->device_id != devicePtr->device_id || 
                device->bus.bus_type != devicePtr->bus.bus_type) 
                continue;
            if (StringCompare(device->device_name, DEVICE_NAME_LEN, devicePtr->device_name, DEVICE_NAME_LEN) == 0) 
                continue;
            
            // Link the hardware addresses
            device->hardware_slot = devicePtr->hardware_slot;
            
            device->hardware_address = devicePtr->hardware_address;
            
            driver->is_linked = 1;
            
            break;
        }
        
    }
    
    return;
}


struct Device* GetDeviceByIndex(uint8_t index) {
    struct Node* node = ListGetNode(DeviceTableHead, index);
    if (!node) 
        return NULL;
    return (struct Device*)node->data;
}


uint8_t GetNumberOfDevices(void) {
    
    return ListGetSize(DeviceTableHead);
}

void DeviceBusyWait(struct Device* devicePtr, uint8_t deviceID) {
    uint8_t checkByte = 0;
    bus_read_byte(&deviceBus, devicePtr->hardware_address, &checkByte);
    while (checkByte != deviceID) {
        bus_read_byte(&deviceBus, devicePtr->hardware_address, &checkByte);
    }
    return;
}

