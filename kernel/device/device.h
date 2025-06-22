#ifndef __DEVICE_TABLE_
#define __DEVICE_TABLE_

#include <kernel/kconfig.h>

#include <kernel/bus/bus.h>

#define DEVICE_NAME_LEN   10



struct Device {
    
    uint8_t device_name[10];         /// Name used for identification     
    uint8_t device_id;               /// Software interrupt ID
    uint8_t device_class;            /// Type of device(storage, networking, ...)
    
    uint8_t  hardware_slot;          /// Physical peripheral slot where the device is located.
    uint32_t hardware_address;       /// Hardware address pointing to the device on the bus.
    
    struct Bus bus;                  /// Hardware bus with which this device is associated.
    
};


void InitiateDeviceTable(void);

struct Device* GetDeviceByIndex(uint8_t index);

uint8_t GetNumberOfDevices(void);

void DeviceBusyWait(struct Device* devicePtr, uint8_t deviceID);

#endif
