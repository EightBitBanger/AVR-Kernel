#ifndef NETWORK_ROUTER_APPLICATION
#define NETWORK_ROUTER_APPLICATION

#include <kernel/network/network.h>

#define CLIENT_TYPE_UNKNOWN   0
#define CLIENT_TYPE_NODE      1
#define CLIENT_TYPE_ROUTER    2


struct RoutingTable {
	
	uint8_t device_state[5];
	uint8_t device_type[5];
	
	uint8_t address_low[5];
	uint8_t address_high[5];
    
};

void InitiateRouter(void);

#endif
