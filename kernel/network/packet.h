#ifndef __NETWORK_PACKETING_
#define __NETWORK_PACKETING_

#include <kernel/bus/bus.h>
#include <kernel/device/device.h>
#include <kernel/driver/driver.h>

#include <kernel/cstring.h>


#define NETWORK_PACKET_SIZE         sizeof(struct NetworkPacket)
 
#define NETWORK_PACKET_DATA_SIZE    16

#define NETWORK_PACKET_START_BYTE   0x55
#define NETWORK_PACKET_TYPE_DATA    0x00

#define NETWORK_PACKET_STOP_BYTE    0xaa


struct NetworkPacket {
	
	uint8_t start;       // Start byte
	
	uint8_t addr_d[2];   // Destination device address
	uint8_t addr_s[2];   // Source device address
	
	uint8_t index;       // Current packet in this chain
	uint8_t total;       // Total number of packets in this chain
	
	// Data block
	uint8_t data[NETWORK_PACKET_DATA_SIZE];
	
	uint8_t stop;        // Stop byte
	
};

uint8_t ntPacketSend(struct NetworkPacket* packet);

uint8_t ntPacketReceive(struct NetworkPacket* packet, uint8_t clearOldPacket);

uint8_t ntPacketClearAll(void);

#endif
