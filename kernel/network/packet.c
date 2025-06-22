#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/network/network.h>
#include <kernel/network/packet.h>

extern struct Driver* networkDevice;

uint8_t ntPacketSend(struct NetworkPacket* packet) {
    
    // Start byte to indicate the beginning of a packet
    ntSend(&packet->start, 1);
    
    // Send the destination and source address
    ntSend(&packet->addr_d[0], 2);
    ntSend(&packet->addr_s[0], 2);
    
    // Packet indexing
    ntSend(&packet->index, 1);
    ntSend(&packet->total, 1);
    
    ntSend(&packet->data[0], NETWORK_PACKET_DATA_SIZE);
    
    // Stop byte to indicate the packet is complete
    ntSend(&packet->stop, 1);
    
    return 1;
}

uint8_t ntPacketReceive(struct NetworkPacket* packet, uint8_t clearOldPacket) {
    
    uint8_t buffer[80];
    
    for (uint8_t i=0; i < 80; i++) 
        networkDevice->read( 0x10 + i, &buffer[i]);
    
    // Find a packet start byte
    for (uint8_t i=0; i < 80; i++) {
        
        if (buffer[i] == NETWORK_PACKET_START_BYTE) {
            
            uint8_t packetEnd = 1 + 2 + 2 + 1 + 1 + NETWORK_PACKET_DATA_SIZE;
            
            if (buffer[i + packetEnd] == NETWORK_PACKET_STOP_BYTE) {
                
                packet->start = NETWORK_PACKET_START_BYTE;
                
                packet->addr_d[0] = buffer[i + 1];
                packet->addr_d[1] = buffer[i + 2];
                packet->addr_s[0] = buffer[i + 3];
                packet->addr_s[1] = buffer[i + 4];
                
                packet->index = buffer[i + 5];
                packet->total = buffer[i + 6];
                
                for (uint8_t d=0; d < NETWORK_PACKET_DATA_SIZE; d++) 
                    packet->data[d] = buffer[7 + i + d];
                
                packet->stop = NETWORK_PACKET_STOP_BYTE;
                
                // Clear the old packet
                if (clearOldPacket == 1) {
                    
                    for (uint8_t c=0; c <= NETWORK_PACKET_SIZE; c++) 
                        networkDevice->write( 0x10 + i + c, 0x00);
                    
                }
                
                return 1;
            }
            
        }
        
    }
    
    return 0;
}

uint8_t ntPacketClearAll(void) {
    
    uint8_t receiveBufferSz = 0;
    
    for (uint8_t i=0; i < 200; i++) 
        networkDevice->write( 0x10 + i, 0x00);
    
    networkDevice->write( 0x0b, receiveBufferSz);
    
    return 1;
}

