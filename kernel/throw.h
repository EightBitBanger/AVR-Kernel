#ifndef __KERNEL_ERROR_HANDLING_
#define __KERNEL_ERROR_HANDLING_

#define HALT_SEGMENTATION_FAULT   0x80000004
#define HALT_OUT_OF_MEMORY        0x10000008
#define HALT_BAD_ALLOCATION       0x50000007
#define HALT_ACCESS_VIOLATION     0x80000064
#define HALT_APP_HANG             0x10000002


void kThrow(long int errorCode, uint32_t hardwareAddress);


#endif
