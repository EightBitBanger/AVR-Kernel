#include <kernel/kernel.h>
#include <kernel/driver/driver.h>

struct Node* DriverTableHead = NULL;

int8_t LoadLibrary(uint8_t* filename, uint8_t filenameLength) {
    /*
    uint32_t fileAddress = fsFileExists(filename, filenameLength);
    
    if (fileAddress == 0) 
        return -1;
    
    int32_t index = fsFileOpen(fileAddress);
    uint32_t fileSize = fsFileGetSize(fileAddress);
    
    uint8_t fileBuffer[fileSize];
    fsFileRead(index, fileBuffer, fileSize);
    fsFileClose(index);
    
    // Check driver corruption
    if (fileBuffer[0] != 'K' || 
        fileBuffer[1] != 'D' || 
        fileBuffer[2 + DEVICE_NAME_LEN] != '$') {
        return -2;
    }
    
    // Check if a driver is already servicing the hardware device
    // that this driver is targeting
    
    uint32_t numberOfDrivers = ListGetSize(DriverTableHead);
    for (uint8_t i=0; i < numberOfDrivers; i++) {
        
        struct Driver* driverPtr = GetDriverByIndex(i);
        
        // Check driver name
        uint8_t found=1;
        for (uint8_t n=0; n < DEVICE_NAME_LEN; n++) {
            if (driverPtr->device.device_name[n] == fileBuffer[2 + n]) 
                continue;
            found = 0;
            break;
        }
        
        if (found == 1) 
            return -3;
        
        continue;
    }
     
    // Construct the driver and register it
    
    struct Driver* newDeviceDriver = (struct Driver*)malloc(sizeof(struct Driver));
    RegisterDriver( (void*)newDeviceDriver );
    
    // Name
    for (uint8_t a=0; a < DEVICE_NAME_LEN; a++) 
        newDeviceDriver->device.device_name[a] = fileBuffer[2 + a];
    
    // Default non-linked blank driver
    newDeviceDriver->is_linked = 0;
    newDeviceDriver->device.hardware_slot = 0;
    
    // Set the bus interface type
    newDeviceDriver->device.device_id            = fileBuffer[2 + DEVICE_NAME_LEN + 1];
    newDeviceDriver->device.bus.read_waitstate   = fileBuffer[2 + DEVICE_NAME_LEN + 2];
    newDeviceDriver->device.bus.write_waitstate  = fileBuffer[2 + DEVICE_NAME_LEN + 3];
    newDeviceDriver->device.bus.bus_type         = fileBuffer[2 + DEVICE_NAME_LEN + 4];
    
    switch (newDeviceDriver->device.bus.bus_type) {
        
        default:
            
        case 0:
            newDeviceDriver->read  = (void(*)(uint32_t, uint8_t*)) bus_read_byte;
            newDeviceDriver->write = (void(*)(uint32_t, uint8_t))  bus_write_byte;
            break;
            
        case 1:
            newDeviceDriver->read  = (void(*)(uint32_t, uint8_t*)) bus_read_memory;
            newDeviceDriver->write = (void(*)(uint32_t, uint8_t))  bus_write_memory;
            break;
            
        case 2:
            newDeviceDriver->read  = (void(*)(uint32_t, uint8_t*)) bus_read_byte;
            newDeviceDriver->write = (void(*)(uint32_t, uint8_t))  bus_write_byte;
            break;
        
    }
    
    // Get hardware address pointer
    union Pointer addrPtr;
    addrPtr.byte_t[0] = fileBuffer[2 + DEVICE_NAME_LEN + 7];
    addrPtr.byte_t[1] = fileBuffer[2 + DEVICE_NAME_LEN + 8];
    addrPtr.byte_t[2] = fileBuffer[2 + DEVICE_NAME_LEN + 9];
    addrPtr.byte_t[3] = fileBuffer[2 + DEVICE_NAME_LEN + 10];
    
    newDeviceDriver->device.hardware_address = addrPtr.address;
    
    
    // Link the driver to any associated hardware
    // devices if its found the bus
    uint8_t numberOfDevices = GetNumberOfDevices();
    
    for (uint8_t d=0; d < numberOfDevices; d++) {
        
        struct Device* devicePtr = GetDeviceByIndex(d);
        
        if (!StringCompare(devicePtr->device_name, DEVICE_NAME_LEN, &fileBuffer[2], DEVICE_NAME_LEN)) 
            continue;
        
        newDeviceDriver->is_linked = 1;
        
        newDeviceDriver->device.hardware_slot    = devicePtr->hardware_slot;
        newDeviceDriver->device.hardware_address = devicePtr->hardware_address;
        
        return 4;
    }
    */
    return 0;
}

struct Driver* GetDriverByName(uint8_t* name, uint8_t nameLength) {
	uint32_t numberOfDrivers = ListGetSize(DriverTableHead);
	
	for (uint8_t index=0; index < numberOfDrivers; index++) {
        
        struct Driver* driverPtr = GetDriverByIndex(index);
        
        if (StringCompare(driverPtr->device.device_name, nameLength, name, nameLength) == 1) 
            return driverPtr;
        
        continue;
	}
	
	return nullptr;
}

struct Driver* GetDriverByIndex(uint8_t index) {
    struct Node* node = ListGetNode(DriverTableHead, index);
    return (struct Driver*)node->data;
}

uint32_t RegisterDriver(void* deviceDriverPtr) {
    ListAddNode(&DriverTableHead, deviceDriverPtr);
    return ListGetSize(DriverTableHead);
}

uint32_t GetNumberOfDrivers(void) {
    return ListGetSize(DriverTableHead);
}


