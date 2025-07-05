#include <kernel/kernel.h>

#include <kernel/delay.h>


#define HARDWARE_INTERRUPT_TABLE_SIZE  8

void (*hardware_interrupt_table[HARDWARE_INTERRUPT_TABLE_SIZE])();

struct Bus isr_bus;

uint32_t dirProcAddress;

struct VirtualFileSystemInterface vfs;


void kInit(void) {
    kPrintVersion();
    
    // Initiate console prompt
    uint8_t prompt[] = "x>";
    
    ConsoleSetPrompt(prompt, sizeof(prompt));
    
    // Initiate memory storage
    fsDeviceSetBase(0x00000);
    
    struct Partition part = fsDeviceOpen(0x00000);
    fsDeviceFormat(&part, 0, 32768, 32, FS_DEVICE_TYPE_MEMORY, (uint8_t*)"master");
    DirectoryHandle rootDirectory = fsDeviceGetRootDirectory(part);
    
    fsWorkingDirectorySetRoot(part, rootDirectory);
    
    uint8_t devDirectoryName[] = "dev";
    DirectoryHandle devDirectoryHandle = fsDirectoryCreate(part, devDirectoryName);
    fsDirectoryAddFile(part, rootDirectory, devDirectoryHandle);
    
    uint8_t procDirectoryName[] = "proc";
    DirectoryHandle procDirectoryHandle = fsDirectoryCreate(part, procDirectoryName);
    fsDirectoryAddFile(part, rootDirectory, procDirectoryHandle);
    
    // List devices on the bus
    
    for (uint8_t index=0; index < NUMBER_OF_PERIPHERALS; index++) {
        struct Device* devPtr = GetDeviceByIndex( index );
        if (devPtr->device_id == 0) 
            continue;
        
        uint8_t length = 0;
        for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) {
            if (devPtr->device_name[i] == ' ') {
                length = i + 1;
                break;
            }
        }
        if (length == 0) 
            continue;
        
        // Create device reference file
        uint32_t fileHandle = fsFileCreate(part, devPtr->device_name, 40);
        fsDirectoryAddFile(part, devDirectoryHandle, fileHandle);
        
        uint8_t attrib[] = {'s','r','w',' '};
        fsFileSetAttributes(part, fileHandle, attrib);
        
        // Initiate file
        uint32_t fileSize = fsFileGetSize(part, fileHandle);
        uint8_t fileBuffer[fileSize];
        for (unsigned int i=0; i < fileSize; i++) 
            fileBuffer[i] = ' ';
        
        // Device file header
        fileBuffer[0] = 'K';
        fileBuffer[1] = 'D';
        fileBuffer[2] = 'E';
        fileBuffer[3] = 'V';
        fileBuffer[4] = '\n';
        
        // Slot
        fileBuffer[5] = 's';
        fileBuffer[6] = 'l';
        fileBuffer[7] = 'o';
        fileBuffer[8] = 't';
        fileBuffer[9] = '=';
        fileBuffer[10] = devPtr->hardware_slot + '1';
        fileBuffer[11] = '\n';
        
        // Address
        fileBuffer[12] = '0';
        fileBuffer[13] = 'x';
        int_to_hex_string(devPtr->hardware_address, &fileBuffer[14]);
        
        File fileIndex = fsFileOpen(part, fileHandle);
        fsFileWrite(part, fileIndex, fileBuffer, fileSize);
        fsFileClose(fileIndex);
        
        continue;
    }
    
    
    // Find an active storage device on the bus
    fsDeviceSetCurrent( 0x00000 );
    struct Partition devicePart;
    devicePart.block_address=0;
    
    for (uint8_t index=0; index < NUMBER_OF_PERIPHERALS; index++) {
        // Get the root directory from the storage device
        uint32_t deviceAddress = PERIPHERAL_ADDRESS_BEGIN + (index * PERIPHERAL_STRIDE);
        fsDeviceSetBase( deviceAddress );
        
        devicePart = fsDeviceOpen( 0x00000 );
        if (devicePart.block_size == 0) 
            continue;
        
        // Set the root
        fsWorkingDirectorySetRoot( devicePart, fsDeviceGetRootDirectory(devicePart));
        
        uint8_t consolePrompt[] = "x>";
        consolePrompt[0] = index + 'a';
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
        fsEnvironmentSetHomeDevice(deviceAddress);
        break;
    }
    
    // If no devices where found use the memory device
    if (devicePart.block_size == 0) {
        fsDeviceSetBase(0x00000);
        
        uint8_t consolePrompt[] = "x>";
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
    }
    
    //
    // Load drivers
    
#ifndef _BOOT_SAFEMODE__
    devicePart = fsDeviceOpen(0x00000);
    //DirectoryHandle rootDeviceDir = fsDeviceGetRootDirectory(devicePart);
    
    // Locate drivers directory
    uint8_t driversDirName[] = "sys";
    uint32_t directoryAddress = fsDirectoryFindByName(devicePart, rootDirectory, driversDirName);
    
    if (directoryAddress == 0) 
        return;
    
#endif
    return;
}

void KernelVectorTableInit(void) {
    isr_bus.read_waitstate  = 1;
    
    for (uint8_t i=0; i < HARDWARE_INTERRUPT_TABLE_SIZE; i++) 
        hardware_interrupt_table[i] = nullfunction;
    
    return;
}

void SetInterruptVector(uint8_t index, void(*servicePtr)()) {
    
    hardware_interrupt_table[ index ] = servicePtr;
    
    return;
}

struct Mutex isr_mux;

void _ISR_hardware_service_routine(void) {
    MutexLock(&isr_mux);
    
    uint8_t msgTest[] = "A";
    print(msgTest, sizeof(msgTest));
    printSpace(1);
    //printLn();
    
    MutexUnlock(&isr_mux);
    return;
    
    uint8_t vect = 0;
    bus_read_byte(&isr_bus, 0x00000, &vect);
    
    if (((vect >> 7) & 1) != 0) {hardware_interrupt_table[0]();}
    if (((vect >> 6) & 1) != 0) {hardware_interrupt_table[1]();}
    if (((vect >> 5) & 1) != 0) {hardware_interrupt_table[2]();}
    if (((vect >> 4) & 1) != 0) {hardware_interrupt_table[3]();}
    if (((vect >> 3) & 1) != 0) {hardware_interrupt_table[4]();}
    if (((vect >> 2) & 1) != 0) {hardware_interrupt_table[5]();}
    if (((vect >> 1) & 1) != 0) {hardware_interrupt_table[6]();}
    if (((vect >> 0) & 1) != 0) {hardware_interrupt_table[7]();}
    
    MutexUnlock(&isr_mux);
    return;
}


void nullfunction(void) {}
