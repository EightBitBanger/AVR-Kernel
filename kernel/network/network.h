#ifndef __NETWORK_INTERFACE_
#define __NETWORK_INTERFACE_

#include <kernel/bus/bus.h>
#include <kernel/device/device.h>
#include <kernel/driver/driver.h>

#include <kernel/cstring.h>

#define NETWORK_BAUD_RATE   12


uint8_t ntInit(void);

void ntSend(uint8_t* buffer, uint8_t bufferSize);
uint8_t ntReceive(uint8_t* buffer, uint8_t bufferSize);
void ntReceiveClear(void);

void ntSetBaudRate(uint8_t baudRate);
uint8_t ntGetBaudRate(void);

void ntBindDevice(uint8_t deviceIndex);

uint8_t ntCheckDevice(void);

uint8_t ntCheckInitiated(void);


#endif
