//
// Hardware configuration

// --------------------------------------------
// Target board
//

#define  BOARD_RETROBOARD_REV2
//#define  BOARD_RETRO_AVR_X4_REV1


// --------------------------------------------
// Boot options

//#define _BOOT_SAFEMODE__
#define _BOOT_DETAILS__

#define _KERNEL_PRINT_VERSION_INFORMATION__


// --------------------------------------------
// INTERNAL



// --------------------------------------------
// Hardware maps

#ifdef BOARD_RETROBOARD_REV2
    
    #define EXTERNAL_MEMORY_BEGIN     0x00000
    #define MAX_MEMORY_SIZE           0x40000
    
    #define PERIPHERAL_ADDRESS_BEGIN  0x40000
    #define PERIPHERAL_STRIDE         0x10000
    
    #define NUMBER_OF_PERIPHERALS  5
    
#endif

#ifdef BOARD_RETRO_AVR_X4_REV1
    
    #define EXTERNAL_MEMORY_BEGIN     0x00000
    #define MAX_MEMORY_SIZE           0x80000
    
    #define PERIPHERAL_ADDRESS_BEGIN  0x10000
    #define PERIPHERAL_STRIDE         0x10000
    
    #define NUMBER_OF_PERIPHERALS  5
    
#endif

