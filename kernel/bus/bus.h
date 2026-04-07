#ifndef __BUS_INTERFACE_
#define __BUS_INTERFACE_

#include <stdint.h>

struct Bus {
    uint16_t read_waitstate;
    uint16_t write_waitstate;
};

#endif
