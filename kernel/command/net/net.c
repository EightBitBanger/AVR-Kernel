#include <avr/io.h>
#include <kernel/delay.h>

#include <kernel/kernel.h>

#include <kernel/network/network.h>
#include <kernel/network/packet.h>

#include <kernel/command/net/net.h>

uint8_t clientAddress[2] = {24, 0};
uint8_t targetAddress[2] = {0x80, 0x80};
uint8_t isInitiated = 0;

void functionNet(uint8_t* param, uint8_t param_length) {
    
    uint8_t msgConnected[]        = "Connected to host";
    uint8_t msgReplyFrom[]        = "Reply from ";
    uint8_t msgNoHostResponce[]   = "Cannot reach host";
    uint8_t msgRequestTimedOut[]  = "Request timed out";
    uint8_t msgNoMessages[]       = "0 messages";
    uint8_t msgDeviceNoInit[]     = "Device driver error";
    uint8_t msgInitFail[]         = "Initiation failed";
    
    
    // Check network device initiated
    if (isInitiated == 0) {
        if (ntInit() == 0) {
            print(msgInitFail, sizeof(msgInitFail));
            printLn();
            return;
        }
        isInitiated = 1;
    }
    
    if (ntCheckInitiated() == 0) {
        print(msgDeviceNoInit, sizeof(msgDeviceNoInit));
        printLn();
        return;
    }
    
    
    uint8_t buffer[32];
    ntSend(buffer, sizeof(buffer));
    
    
    
    
    
    
    return;
    
    // Lower case the string
    for (uint8_t i=0; i < 4; i++) 
        lowercase( &param[i] );
    
    //
    // Initiate the network card
    //
    
    if (isInitiated == 0) {
        
        if (ntInit() == 0) {
            
            print(msgInitFail, sizeof(msgInitFail));
            
            printLn();
            
            return;
        }
        
        isInitiated = 1;
        
    }
    
    //
    // Check device initiated
    
    if (ntCheckInitiated() == 0) {
        print(msgDeviceNoInit, sizeof(msgDeviceNoInit));
        printLn();
        return;
    }
    
    
    //
    // Set the baud rate
    //
    
    if ((param[0] == 'b') & (param[1] == 'a') & (param[2] == 'u') & (param[3] == 'd')) {
        
        uint8_t msg[] = "Baud set =     ";
        
        if (param_length < 5) {
            
            uint8_t baudrate = ntGetBaudRate();
            
            uint8_t baudRateString[10];
            uint8_t place = int_to_string(baudrate, baudRateString);
            
            for (uint8_t i=0; i < place + 1; i++) 
                msg[11 + i] = baudRateString[i];
            
        } else {
            
            if (param[5] == '0') {msg[11] = '9'; msg[12] = '0'; msg[13] = '0';                ntSetBaudRate(0);}
            if (param[5] == '1') {msg[11] = '1'; msg[12] = '2'; msg[13] = '0'; msg[14] = '0'; ntSetBaudRate(1);}
            if (param[5] == '2') {msg[11] = '2'; msg[12] = '4'; msg[13] = '0'; msg[14] = '0'; ntSetBaudRate(2);}
            if (param[5] == '3') {msg[11] = '4'; msg[12] = '8'; msg[13] = '0'; msg[14] = '0'; ntSetBaudRate(3);}
            if (param[5] == '4') {msg[11] = '9'; msg[12] = '6'; msg[13] = '0'; msg[14] = '0'; ntSetBaudRate(4);}
            if (param[5] == '5') {msg[11] = '2'; msg[12] = '8'; msg[13] = 'K';                ntSetBaudRate(5);}
            if (param[5] == '6') {msg[11] = '3'; msg[12] = '3'; msg[13] = 'K';                ntSetBaudRate(6);}
            if (param[5] == '7') {msg[11] = '5'; msg[12] = '6'; msg[13] = 'K';                ntSetBaudRate(7);}
            if (param[5] == '8') {msg[11] = '7'; msg[12] = '6'; msg[13] = 'K';                ntSetBaudRate(8);}
            if (param[5] == '9') {msg[11] = '1'; msg[12] = '2'; msg[13] = '5'; msg[14] = 'K'; ntSetBaudRate(9);}
            
        }
        
        //ConsoleSetCursorPosition();
        
        print(msg, sizeof(msg));
        printLn();
        
        return;
    }
    
    
    //
    // Send the parameter message
    //
    
    if ((param[0] == 's') & (param[1] == 'e') & (param[2] == 'n') & (param[3] == 'd')) {
        
        struct NetworkPacket packet;
        
        for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
            packet.data[i] = ' ';
        
        packet.start   = NETWORK_PACKET_START_BYTE;
        
        packet.addr_d[0]  = targetAddress[0]; // Destination address
        packet.addr_d[1]  = targetAddress[1];
        packet.addr_s[0]  = clientAddress[0]; // Return source address
        packet.addr_s[1]  = clientAddress[1];
        
        packet.index = 1;
        packet.total = 1;
        
        uint8_t stringLen = (param_length - 4) + 1;
        
        for (uint8_t i=0; i < stringLen; i++) 
            packet.data[i] = param[5 + i];
        
        packet.stop  = NETWORK_PACKET_STOP_BYTE;
        
        ntPacketClearAll();
        
        // Transmit the packet
        //ntPacketSend(&packet);
        
        
        //
        // Check receiver packet
        
        struct NetworkPacket receive;
        
        for (uint8_t retry=0; retry < 3; retry++) {
            
            // Transmit the packet
            ntPacketSend(&packet);
            
            for (uint16_t i=0; i < 1024; i++) {
                
                _delay_ms(1);
                if (ntPacketReceive(&receive, 1) == 0) 
                    continue;
                
                uint8_t index = receive.index;
                uint8_t total = receive.total;
                
                print(&receive.data[0], NETWORK_PACKET_DATA_SIZE);
                printLn();
                
                // Check last packet
                if (index == total) {
                    
                    ntPacketClearAll();
                    
                    return;
                }
                
                continue;
            }
            
        }
        
        print(&msgNoHostResponce[0], sizeof(msgNoHostResponce));
        printLn();
        
        return;
    }
    
    
    //
    // Send the parameter message
    //
    
    if ((param[0] == 'r') & (param[1] == 'e') & (param[2] == 'c') & (param[3] == 'v')) {
        
        struct NetworkPacket packet;
        
        if (ntPacketReceive(&packet, 1) == 0) {
            
            print(&msgNoMessages[0], sizeof(msgNoMessages));
            printLn();
            
            ntPacketClearAll();
            
            return;
        }
        
        print(&packet.data[0], NETWORK_PACKET_DATA_SIZE);
        printLn();
        
        return;
    }
    
    
    //
    // Connect to a server, adding your address to the routing table
    //
    
    if ((param[0] == 'c') & (param[1] == 'o') & (param[2] == 'n')) {
        
        struct NetworkPacket packet;
        
        for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
            packet.data[i] = 0x55;
        
        packet.start   = NETWORK_PACKET_START_BYTE;
        
        packet.addr_d[0]   = 0xff; // Destination
        packet.addr_d[1]   = 0xff;
        packet.addr_s[0]   = clientAddress[0]; // Source
        packet.addr_s[1]   = clientAddress[1];
        
        packet.index = 1;
        packet.total = 1;
        
        // Indicate we are a client attempting to connect to a server
        packet.data[0] = 0x55;
        packet.data[1] = 0x00;
        
        packet.stop    = NETWORK_PACKET_STOP_BYTE;
        
        ntReceiveClear();
        
        // Transmit the packet
        ntPacketSend(&packet);
        
        uint8_t bufferSz = 0;
        
        for (uint32_t i=0; i < 1024; i++) {
            
            uint8_t dataBuffer[32];
            
            bufferSz = ntReceive(dataBuffer, 32);
            
            if (bufferSz > (sizeof(struct NetworkPacket) - 1)) {
                
                // Check return packet
                if (dataBuffer[0]  != NETWORK_PACKET_START_BYTE) continue;
                if (dataBuffer[1 + 2 + 2 + 1 + 1 + NETWORK_PACKET_DATA_SIZE] != NETWORK_PACKET_STOP_BYTE) continue;
                
                if (dataBuffer[3] != 0xff) continue;
                if (dataBuffer[4] != 0xff) continue;
                
                uint8_t addrStrLow[8];
                uint8_t addrStrHigh[8];
                
                int_to_string(packet.addr_d[0], addrStrLow);
                int_to_string(packet.addr_d[1], addrStrHigh);
                
                print(msgConnected, sizeof(msgConnected));
                printLn();
                
                break;
            }
            
            _delay_ms(1);
            
        }
        
        if (bufferSz == 0) {
            
            print(msgNoHostResponce, sizeof(msgNoHostResponce));
            printLn();
            
        }
        
        return;
    }
    
    
    //
    // Ping the server
    //
    
    if ((param[0] == 'p') & (param[1] == 'i') & (param[2] == 'n') & (param[3] == 'g')) {
        
        struct NetworkPacket packet;
        
        for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
            packet.data[i] = 0x55;
        
        packet.start   = NETWORK_PACKET_START_BYTE;
        
        packet.addr_d[0]   = 0xff; // To the router
        packet.addr_d[1]   = 0xff;
        packet.addr_s[0]   = 0x00; // Ping request
        packet.addr_s[1]   = 0x00;
        
        packet.index = 1;
        packet.total = 1;
        
        // Indicate we are requesting a ping from the server
        packet.data[0] = 0x55;
        packet.data[1] = 0x00;
        
        packet.stop    = NETWORK_PACKET_STOP_BYTE;
        
        ntReceiveClear();
        
        for (uint8_t i=0; i < 1; i++) {
            
            ntPacketSend(&packet);
            
            uint8_t dataBuffer[NETWORK_PACKET_SIZE];
            
            uint8_t bufferSz = 0;
            
            for (uint16_t i=0; i < 1024; i++) {
                
                bufferSz = ntReceive(dataBuffer, NETWORK_PACKET_SIZE);
                
                if (bufferSz > NETWORK_PACKET_SIZE - 1) {
                    
                    // Check return packet
                    if ((dataBuffer[0]  != NETWORK_PACKET_START_BYTE) | 
                        (dataBuffer[1 + 2 + 2 + 1 + 1 + NETWORK_PACKET_DATA_SIZE] != NETWORK_PACKET_STOP_BYTE)) 
                        continue;
                    
                    
                    uint8_t addrStrLow[8];
                    uint8_t addrStrHigh[8];
                    
                    uint8_t lenA = int_to_string(packet.addr_d[0], addrStrLow);
                    uint8_t lenB = int_to_string(packet.addr_d[1], addrStrHigh);
                    
                    print(msgReplyFrom, sizeof(msgReplyFrom));
                    
                    ConsoleSetCursorPosition(sizeof(msgReplyFrom)-1);
                    print(addrStrLow, 4);
                    
                    ConsoleSetCursorPosition(sizeof(msgReplyFrom)+lenA-1);
                    uint8_t msgPeriod[] = ".";
                    print(msgPeriod, sizeof(msgPeriod));
                    
                    ConsoleSetCursorPosition(sizeof(msgReplyFrom)+lenA+lenB-3);
                    print(addrStrHigh, 4);
                    
                    printLn();
                    
                    break;
                }
                
                _delay_ms(1);
                
            }
            
            if (bufferSz > 0) 
                continue;
            
            // Connection timed out
            
            _delay_ms(1000);
            
            print(msgRequestTimedOut, sizeof(msgRequestTimedOut));
            printLn();
            
            continue;
        }
        
        return;
    }
    
    //
    // Hammer the network with continuous packets
    //
    
    if ((param[0] == 'r') & (param[1] == 'e') & (param[2] == 'p')) {
        
        while (1) {
            
            uint8_t offset = 10;
            
            struct NetworkPacket packet;
            
            for (uint8_t i=0; i < NETWORK_PACKET_DATA_SIZE; i++) 
                packet.data[i] = 0x55;
            
            packet.start   = NETWORK_PACKET_START_BYTE;
            
            packet.addr_d[0]   = targetAddress[0];
            packet.addr_d[1]   = targetAddress[1];
            packet.addr_s[0]   = clientAddress[0];
            packet.addr_s[1]   = clientAddress[1];
            
            packet.index = 1;
            packet.total = 1;
            
            packet.data[0] = 0x00;
            packet.data[1] = 0x00;
            
            for (uint8_t i=0; i < (param_length - offset) + 1; i++) 
                packet.data[i] = param[offset + i];
            
            packet.stop    = NETWORK_PACKET_STOP_BYTE;
            
            ntPacketSend(&packet);
            
            _delay_ms(100);
            
        }
        
    }
    
    
    
    //
    // Source and destination
    //
    
    if ((param[0] == 's') & (param[1] == 'r') & (param[2] == 'c')) {
        
        uint8_t periodIndex = 0;
        for (uint8_t i=0; i < param_length; i++) {
            if (param[i] == '.') {
                periodIndex = i;
                break;
            }
        }
        
        if (periodIndex > 0) {
            
            uint8_t firstNumber[3]  = {' ', ' ', ' '};
            uint8_t secondNumber[3] = {' ', ' ', ' '};
            
            uint8_t secondNumberLen = param_length - periodIndex;
            
            for (uint8_t i=0; i < 4; i++) {
                if (param[4 + i] == '.') 
                    break;
                firstNumber[i] = param[4 + i];
            }
            
            for (uint8_t i=0; i < secondNumberLen; i++) 
                secondNumber[i] = param[periodIndex + 1 + i];
            
            clientAddress[0] = string_get_int( &firstNumber[0] );
            clientAddress[1] = string_get_int( &secondNumber[0] );
            
        }
        
        uint8_t msgAddressDestA[10];
        uint8_t msgAddressDestB[10];
        
        uint8_t placeDstA = int_to_string(clientAddress[0], msgAddressDestA);
        uint8_t placeDstB = int_to_string(clientAddress[1], msgAddressDestB);
        
        if (placeDstA == 0) placeDstA++;
        if (placeDstB == 0) placeDstB++;
        
        print(msgAddressDestA, placeDstA+1);
        ConsoleSetCursorPosition(placeDstA);
        uint8_t msgPeriod[] = ".";
        print(msgPeriod, sizeof(msgPeriod));
        ConsoleSetCursorPosition(placeDstA+1);
        print(msgAddressDestB, placeDstB+1);
        
        printLn();
        
        return;
    }
    
    if ((param[0] == 'd') & (param[1] == 's') & (param[2] == 't')) {
        
        uint8_t periodIndex = 0;
        for (uint8_t i=0; i < param_length; i++) {
            if (param[i] == '.') {
                periodIndex = i;
                break;
            }
        }
        
        if (periodIndex > 0) {
            
            uint8_t firstNumber[3]  = {' ', ' ', ' '};
            uint8_t secondNumber[3] = {' ', ' ', ' '};
            
            uint8_t secondNumberLen = param_length - periodIndex;
            
            for (uint8_t i=0; i < 4; i++) {
                if (param[4 + i] == '.') 
                    break;
                firstNumber[i] = param[4 + i];
            }
            
            for (uint8_t i=0; i < secondNumberLen; i++) 
                secondNumber[i] = param[periodIndex + 1 + i];
            
            targetAddress[0] = string_get_int( &firstNumber[0] );
            targetAddress[1] = string_get_int( &secondNumber[0] );
            
        }
        
        uint8_t msgAddressDestA[10];
        uint8_t msgAddressDestB[10];
        
        uint8_t placeDstA = int_to_string(targetAddress[0], msgAddressDestA);
        uint8_t placeDstB = int_to_string(targetAddress[1], msgAddressDestB);
        
        if (placeDstA == 0) placeDstA++;
        if (placeDstB == 0) placeDstB++;
        
        print(msgAddressDestA, placeDstA+1);
        ConsoleSetCursorPosition(placeDstA);
        uint8_t msgPeriod[] = ".";
        print(msgPeriod, sizeof(msgPeriod));
        ConsoleSetCursorPosition(placeDstA+1);
        print(msgAddressDestB, placeDstB+1);
        
        printLn();
        
        return;
    }
    
    return;
}


void registerCommandNet(void) {
    
    uint8_t netCommandName[] = "net";
    
    ConsoleRegisterCommand(netCommandName, sizeof(netCommandName), functionNet);
    
    return;
}
