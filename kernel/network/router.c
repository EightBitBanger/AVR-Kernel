#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/network/network.h>
#include <kernel/network/packet.h>

#include <kernel/network/router.h>

struct RoutingTable routingTable;


void InitiateRouter(void) {
    
    uint16_t reconnRequestMax = 2700;
    uint16_t reconnRequestMin = 1000;
    
    uint16_t reconnRequestTimeOut = 0;
    uint16_t reconnRequestRate = reconnRequestMin;
    
    
    
    //
    // Assemble default packets
    //
    
    //
    // Client handshake packet
    
    struct NetworkPacket packetClientHandshake;
    packetClientHandshake.start = NETWORK_PACKET_START_BYTE;
    
    packetClientHandshake.addr_d[0] = 0x00; // To the client
    packetClientHandshake.addr_d[1] = 0x00;
    packetClientHandshake.addr_s[0] = 0xff; // From the router
    packetClientHandshake.addr_s[1] = 0xff;
    
    packetClientHandshake.index = 1;
    packetClientHandshake.total = 1;
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetClientHandshake.data[i] = 0x55;
    
    packetClientHandshake.data[0] = 0x55;
    packetClientHandshake.data[1] = 0x00;
    
    packetClientHandshake.stop = NETWORK_PACKET_STOP_BYTE;
    
    //
    // Router handshake packet
    
    struct NetworkPacket packetRouterHandshakeOutgoing;
    packetRouterHandshakeOutgoing.start = NETWORK_PACKET_START_BYTE;
    
    packetRouterHandshakeOutgoing.addr_d[0] = 0xff; // To the router
    packetRouterHandshakeOutgoing.addr_d[1] = 0xff;
    packetRouterHandshakeOutgoing.addr_s[0] = 0xff; // From a router requesting a connection
    packetRouterHandshakeOutgoing.addr_s[1] = 0xff;
    
    packetRouterHandshakeOutgoing.index = 1;
    packetRouterHandshakeOutgoing.total = 1;
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetRouterHandshakeOutgoing.data[i] = 0x55;
    
    packetRouterHandshakeOutgoing.stop = NETWORK_PACKET_STOP_BYTE;
    
    //
    // Router handshake return packet
    
    struct NetworkPacket packetRouterHandshakeReturn;
    packetRouterHandshakeReturn.start = NETWORK_PACKET_START_BYTE;
    
    packetRouterHandshakeReturn.addr_d[0] = 0xff; // To the router
    packetRouterHandshakeReturn.addr_d[1] = 0xff;
    packetRouterHandshakeReturn.addr_s[0] = 0xff; // From a router requesting a connection
    packetRouterHandshakeReturn.addr_s[1] = 0xff;
    
    packetRouterHandshakeReturn.index = 1;
    packetRouterHandshakeReturn.total = 1;
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetRouterHandshakeReturn.data[i] = 0xAA;
    
    packetRouterHandshakeReturn.stop = NETWORK_PACKET_STOP_BYTE;
    
    
    uint16_t reconnRequestCounter[5] = {0, 0, 0, 0, 0};
    
    
    
    //
    // Initiate the routing table
    //
    for (uint8_t retry=0; retry < 3; retry++) {
        
        for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
            
            if (routingTable.device_state[s] == 1) 
                continue;
            
            routingTable.device_state[s] = 0x00;
            routingTable.device_type[s]  = 0x00;
            
            routingTable.address_low[s]  = 0x00;
            routingTable.address_high[s] = 0x00;
            
            routingTable.device_state[s] = 0;
            routingTable.device_type[s] = 0;
            
            // Initiate NIC baud rate
            ntBindDevice(s);
            
            if (ntCheckDevice() == 0) 
                continue;
            
            ntSetBaudRate( NETWORK_BAUD_RATE );
            
            routingTable.device_state[s] = 1; // Normal active NIC device
            
            routingTable.device_type[s]  = CLIENT_TYPE_UNKNOWN;
            
            continue;
        }
        
        continue;
    }
    
    
    //
    // Connect to adjacent routers to set the default gateway(s)
    //
    for (uint8_t retry=0; retry < 3; retry++) {
        
        for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
            
            if (routingTable.device_state[s] == 0) 
                continue;
            
            ntBindDevice(s);
            
            ntPacketSend( &packetRouterHandshakeOutgoing );
            
        }
        
        continue;
    }
    
    //
    // Router main loop
    //
    
    struct NetworkPacket receive;
    
    while (1) {
        
        for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
            
            ntBindDevice(s);
            
            //
            // Reconnection attempt
            //
            
            reconnRequestCounter[s]++;
            if (reconnRequestCounter[s] > reconnRequestRate) {
                
                reconnRequestCounter[s] = 0;
                
                // Time out counter
                if (s == 0) reconnRequestTimeOut++;
                
                if (reconnRequestTimeOut > 2) {
                    
                    // Lengthen connection request interval
                    reconnRequestRate = reconnRequestMax;
                    
                    if (reconnRequestTimeOut > 10) {
                        
                        reconnRequestRate = reconnRequestMin;
                        reconnRequestTimeOut = 0;
                        
                        //
                        // Initiate a hot inserted NIC card
                        
                        for (uint8_t ds=0; ds < NUMBER_OF_PERIPHERALS; ds++) {
                            
                            if (routingTable.device_type[ds] != CLIENT_TYPE_UNKNOWN) 
                                continue;
                            
                            // Initiate NIC baud rate
                            ntBindDevice(ds);
                            
                            if (ntCheckDevice() == 0) 
                                continue;
                            
                            ntSetBaudRate( NETWORK_BAUD_RATE );
                            
                            routingTable.device_state[ds] = 1;
                            
                        }
                        
                        for (uint8_t ds=0; ds < NUMBER_OF_PERIPHERALS; ds++) {
                            
                            if (routingTable.device_state[ds] == 0) 
                                continue;
                            
                            if (routingTable.device_type[ds] != CLIENT_TYPE_UNKNOWN) 
                                continue;
                            
                            // Initiate NIC baud rate
                            ntBindDevice(ds);
                            
                            ntPacketClearAll();
                            
                            ntPacketSend( &packetRouterHandshakeOutgoing );
                            
                        }
                        
                        ntBindDevice(s);
                        
                    }
                }
                
                // Only reconnect to known router devices
                if (routingTable.device_type[s] == CLIENT_TYPE_ROUTER) {
                    
                    ntPacketClearAll();
                    
                    ntPacketSend( &packetRouterHandshakeOutgoing );
                    
                }
                
            }
            
            if (routingTable.device_state[s] == 0) 
                continue;
            
            receive.addr_d[0] = 0x00; receive.addr_d[1] = 0x00;
            receive.addr_s[0] = 0x00; receive.addr_s[1] = 0x00;
            
            // Check incoming packet
            if (ntPacketReceive(&receive, 1) == 0) 
                continue;
            
            // Check packet address
            if ((receive.addr_d[0] == 0x00) & (receive.addr_d[1] == 0x00) & 
                (receive.addr_s[0] == 0x00) & (receive.addr_s[1] == 0x00)) {
                
                continue;
            }
            
            // Clear old packet data
            ntPacketClearAll();
            
            
            //
            // Incoming to this server
            //
            
            if ((receive.addr_d[0] == 0xff) & 
                (receive.addr_d[1] == 0xff)) {
                
                //
                // Client connection request
                //
                
                if ((receive.data[0] == 0x55) & 
                    (receive.data[1] == 0x00)) {
                    
                    // Send the return packet acknowledging the client connection request
                    ntPacketSend( &packetClientHandshake );
                    
                    // Client is an addressable end point node
                    routingTable.device_type[s] = CLIENT_TYPE_NODE;
                    
                    // Collect the client address
                    routingTable.address_low[s]  = receive.addr_s[0];
                    routingTable.address_high[s] = receive.addr_s[1];
                    
                    continue;
                }
                
                
                //
                // Router connection request
                //
                
                if ((receive.addr_s[0] == 0xff) & 
                    (receive.addr_s[1] == 0xff)) {
                    
                    // Incoming router connection request
                    if (receive.data[0] == 0x55) {
                        
                        routingTable.device_type[s] = CLIENT_TYPE_ROUTER;
                        
                        // Send the return packet acknowledging the router connection request
                        ntPacketSend( &packetRouterHandshakeReturn );
                        
                    }
                    
                    // Return router connection request
                    if (receive.data[0] == 0xAA) {
                        
                        routingTable.device_type[s] = CLIENT_TYPE_ROUTER;
                        
                    }
                    
                }
                
                
                //
                // Ping request
                //
                
                if ((receive.addr_s[0] == 0x00) & 
                    (receive.addr_s[1] == 0x00)) {
                    
                    // Return the ping packet to the sender
                    ntPacketSend( &receive );
                    
                }
                
                continue;
            }
            
            
            //
            // Handle client packets and route them to the destination address
            // If the destination address does not exist the packet will be 
            // routed through the default gateway to the next router(s).
            
            //
            // Check if the destination address node is
            // in this routing table
            
            uint8_t destinationDeviceLocated = 0;
            
            for (uint8_t d=0; d < NUMBER_OF_PERIPHERALS; d++) {
                
                if (routingTable.device_type[d] == CLIENT_TYPE_NODE) {
                    
                    // Find the destination packet address in the routing table
                    if ((receive.addr_d[0] == routingTable.address_low[d]) & 
                        (receive.addr_d[1] == routingTable.address_high[d])) {
                        
                        destinationDeviceLocated = 1;
                        
                        ntBindDevice(d);
                        
                        ntPacketSend(&receive);
                        
                        break;
                    }
                    
                }
                
            }
            
            
            // Do NOT send the packet back out if the destination
            // was located on this routing table
            
            if (destinationDeviceLocated == 1) 
                continue;
            
            
            //
            // Route the packet to any default gateway currently connected
            //
            
            for (uint8_t d=0; d < NUMBER_OF_PERIPHERALS; d++) {
                
                // Do not send the packet back
                // to the sender
                if (s == d) 
                    continue;
                
                // Check if the destination is valid
                if (routingTable.device_type[d] == CLIENT_TYPE_ROUTER) {
                    
                    ntBindDevice(d);
                    
                    ntPacketSend(&receive);
                    
                }
                
            }
            
            continue;
        }
        
        continue;
    }
    
    
    
    
    
    
    
    return;
}







