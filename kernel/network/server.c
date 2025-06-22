#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/network/network.h>
#include <kernel/network/packet.h>

#include <kernel/network/server.h>

uint8_t serverAddress[2] = {240, 80};


void InitiateServer(void) {
    
    ConsoleSetBlinkRate(0);
    ConsoleSetCursor(0, 0);
    
    uint8_t msgString[] = "Server not connected";
    print( msgString, sizeof(msgString) );
    printLn();
    
    uint8_t msgAddressLow[10];
    uint8_t msgAddressHigh[10];
    
    uint8_t placeLow  = int_to_string(serverAddress[0], msgAddressLow);
    uint8_t placeHigh = int_to_string(serverAddress[1], msgAddressHigh);
    
    
    print( msgAddressLow, placeLow+1 );
    printChar('.');
    print( msgAddressHigh, placeHigh+1 );
    printLn();
    
    
    //
    // Client return packet
    
    struct NetworkPacket packetClientReturn;
    packetClientReturn.start = NETWORK_PACKET_START_BYTE;
    
    packetClientReturn.addr_d[0] = 0x00; // To the client address
    packetClientReturn.addr_d[1] = 0x00;
    packetClientReturn.addr_s[0] = serverAddress[0]; // From the server
    packetClientReturn.addr_s[1] = serverAddress[1];
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetClientReturn.data[i] = ' ';
    
    packetClientReturn.stop = NETWORK_PACKET_STOP_BYTE;
    
    //
    // Router connection handshake packet
    
    struct NetworkPacket packetRouterConnectionRequest;
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetRouterConnectionRequest.data[i] = 0x55;
    
    packetRouterConnectionRequest.start   = NETWORK_PACKET_START_BYTE;
    
    packetRouterConnectionRequest.addr_d[0]   = 0xff; // Destination
    packetRouterConnectionRequest.addr_d[1]   = 0xff;
    packetRouterConnectionRequest.addr_s[0]   = serverAddress[0]; // Source
    packetRouterConnectionRequest.addr_s[1]   = serverAddress[1];
    
    // Indicate we are a client attempting to connect to a server
    packetRouterConnectionRequest.data[0] = 0x55;
    packetRouterConnectionRequest.data[1] = 0x00;
    
    packetRouterConnectionRequest.stop    = NETWORK_PACKET_STOP_BYTE;
    
    
    
    
    
    struct NetworkPacket receive;
    
    uint8_t  connectionStatus = 0;
    uint16_t connectionAttemptCounter = 800;
    
    while(1) {
        
        //
        // Connection attempt
        
        if (connectionStatus == 0) {
            
            connectionAttemptCounter++;
            if (connectionAttemptCounter > 800) {
                connectionAttemptCounter = 0;
                
                ntPacketSend( &packetRouterConnectionRequest );
                
            }
            
        }
        
        receive.addr_d[0] = 0x00; receive.addr_d[1] = 0x00;
        receive.addr_s[0] = 0x00; receive.addr_s[1] = 0x00;
        
        // Check incoming packet
        if (ntPacketReceive(&receive, 1) == 0) 
            continue;
        
        // Check packet address
        if ((receive.addr_d[0] == 0x00) & (receive.addr_d[1] == 0x00) & 
            (receive.addr_s[0] == 0x00) & (receive.addr_s[1] == 0x00)) 
            continue;
        
        // Clear old packet data
        ntPacketClearAll();
        
        
        //
        // Incoming packet from the router
        //
        
        if ((receive.addr_s[0] == 0xff) & 
            (receive.addr_s[1] == 0xff)) {
            
            //
            // Client connection confirmation
            //
            
            if ((receive.data[0] == 0x55) & 
                (receive.data[1] == 0x00)) {
                
                connectionStatus = 1;
                
                ConsoleSetCursor(0, 0);
                
                uint8_t msgConnected[] = "Connected           ";
                print( msgConnected, sizeof(msgConnected) );
                
            }
            
            continue;
        }
        
        //
        // Handle client packets
        ConsoleClearScreen(' ');
        
        packetClientReturn.addr_s[0] = serverAddress[0];
        packetClientReturn.addr_s[1] = serverAddress[1];
        
        // Send packet to the sender
        packetClientReturn.addr_d[0] = receive.addr_s[0];
        packetClientReturn.addr_d[1] = receive.addr_s[1];
        
        ConsoleSetCursor(0, 0);
        
        uint8_t msgConnected[] = " <<                ";
        print( msgConnected, sizeof(msgConnected) );
        
        
        uint8_t srcAddressLow[10];
        uint8_t srcAddressHigh[10];
        
        uint8_t addrPlaceLow  = int_to_string(receive.addr_s[0], srcAddressLow);
        uint8_t addrPlaceHigh = int_to_string(receive.addr_s[1], srcAddressHigh);
        
        if (addrPlaceLow == 0) addrPlaceLow++;
        if (addrPlaceHigh == 0) addrPlaceHigh++;
        
        
        // Clear the packet data
        
        for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
            packetClientReturn.data[i] = ' ';
        
        uint8_t messageStringA[NETWORK_PACKET_DATA_SIZE] = "Return packet";
        packetClientReturn.index = 1;
        packetClientReturn.total = 1;
        
        for (uint8_t i=0; i < sizeof(messageStringA); i++) 
            packetClientReturn.data[i] = messageStringA[i];
        
        ntPacketSend( &packetClientReturn );
        
        
        /*
        
        //
        // Send a total of 3 return packets
        
        // Packet first
        uint8_t messageStringA[NETWORK_PACKET_DATA_SIZE] = "PACKETA DEADBEEF";
        packetClientReturn.index = 1;
        packetClientReturn.total = 3;
        
        for (uint8_t i=0; i < sizeof(messageStringA); i++) 
            packetClientReturn.data[i] = messageStringA[i];
        
        ntPacketSend( &packetClientReturn );
        
        
        // Packet second
        uint8_t messageStringB[24] = "PACKETB DEADBEEFDEADBEEF";
        packetClientReturn.index = 2;
        packetClientReturn.total = 3;
        
        for (uint8_t i=0; i < sizeof(messageStringB); i++) 
            packetClientReturn.data[i] = messageStringB[i];
        
        ntPacketSend( &packetClientReturn );
        
        // Packet third
        uint8_t messageStringC[24] = "PACKETC DEADBEEFDEADBEEF";
        packetClientReturn.index = 3;
        packetClientReturn.total = 3;
        
        for (uint8_t i=0; i < sizeof(messageStringC); i++) 
            packetClientReturn.data[i] = messageStringC[i];
        
        ntPacketSend( &packetClientReturn );
        
        */
        
        
        //
        // Display the source address
        
        ConsoleSetCursor(0, 4);
        
        if ((srcAddressLow[0] == ' ') | (srcAddressLow[1] == ' ')) srcAddressLow[0]  = '0';
        if ((srcAddressLow[0] == ' ') | (srcAddressLow[1] == ' ')) srcAddressLow[0]  = '0';
        
        print( srcAddressLow, addrPlaceLow+1 );
        printChar('.');
        print( srcAddressHigh, addrPlaceHigh+1 );
        
        
        //
        // Display packet contents
        
        ConsoleSetCursor(1, 0);
        
        for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
            printChar( receive.data[i] );
        
        continue;
    }
    
    return;
    
    
    
    /*
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
    
    for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
        packetRouterHandshakeReturn.data[i] = 0xAA;
    
    packetRouterHandshakeReturn.stop = NETWORK_PACKET_STOP_BYTE;
    
    
    
    
    uint16_t reconnRequestCounter[5] = {0, 0, 0, 0, 0};
    
    uint16_t reconnRequestMax = 1000;
    
    
    
    //
    // Initiate the routing table
    //
    
    for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
        
        routingTable.device_state[s] = 0x00;
        routingTable.device_type[s]  = 0x00;
        
        routingTable.address_low[s]  = 0x00;
        routingTable.address_high[s] = 0x00;
        
        routingTable.device_state[s] = 0;
        routingTable.device_type[s] = 0;
        
        // Initiate NIC baud rate
        ntBindDevice(s);
        
        _delay_ms(10);
        
        if (ntCheckDevice() == 0) 
            continue;
        
        _delay_ms(10);
        
        ntSetBaudRate( NETWORK_BAUD_RATE );
        
        routingTable.device_state[s] = 1; // Normal active NIC device
        
        routingTable.device_type[s]  = CLIENT_TYPE_UNKNOWN;
        
        continue;
    }
    
    
    
    //
    // Connect to adjacent routers to set the default gateway(s)
    //
    
    for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
        
        if (routingTable.device_state[s] == 0) 
            continue;
        
        ntBindDevice(s);
        
        ntPacketClearAll();
        
        ntPacketSend( &packetRouterHandshakeOutgoing );
        
    }
    
    
    
    struct NetworkPacket receive;
    
    while(1) {
        
        for (uint8_t s=0; s < NUMBER_OF_PERIPHERALS; s++) {
            
            //
            // Reconnection attempt
            //
            
            reconnRequestCounter[s]++;
            if (reconnRequestCounter[s] > reconnRequestMax) {
                
                reconnRequestCounter[s] = 0;
                
                // Only reconnect to known router devices
                if (routingTable.device_type[s] == CLIENT_TYPE_ROUTER) {
                    
                    ntBindDevice(s);
                    
                    ntPacketClearAll();
                    
                    ntPacketSend( &packetRouterHandshakeOutgoing );
                }
                
            }
            
            
            if (routingTable.device_state[s] == 0) 
                continue;
            
            receive.addr_d[0] = 0x00; receive.addr_d[1] = 0x00;
            receive.addr_s[0] = 0x00; receive.addr_s[1] = 0x00;
            
            ntBindDevice(s);
            
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
        
    }
    
    
    
    
    
    
    */
    return;
}







