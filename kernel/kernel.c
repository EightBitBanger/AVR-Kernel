#include <kernel/kernel.h>
#include <kernel/delay.h>

#define HARDWARE_INTERRUPT_VECTOR       0x00000
#define HARDWARE_INTERRUPT_TABLE_SIZE   8

void (*hardware_interrupt_table[HARDWARE_INTERRUPT_TABLE_SIZE])();

void nullfunction(void) {}

struct Bus isr_bus;

uint32_t devDirectoryHandle  = 0;
uint32_t procDirectoryHandle = 0;

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
    uint32_t deviceSize = fsDeviceGetSize(part);
    
    fsWorkingDirectorySetRoot(part, rootDirectory);
    
    uint8_t devDirectoryName[] = "dev";
    devDirectoryHandle = fsDirectoryCreate(part, devDirectoryName);
    fsDirectoryAddFile(part, rootDirectory, devDirectoryHandle);
    
    uint8_t procDirectoryName[] = "proc";
    procDirectoryHandle = fsDirectoryCreate(part, procDirectoryName);
    fsDirectoryAddFile(part, rootDirectory, procDirectoryHandle);
    
    // List devices on the bus
    
    for (uint8_t index=0; index < NUMBER_OF_PERIPHERALS; index++) {
        struct Device* devPtr = GetDeviceByIndex( index );
        if (devPtr->device_id == 0) 
            continue;
        
#ifdef _BOOT_LIST_HARDWARE_DEVICES__
        uint8_t byteHeader[]={'0','x','0','0'+index,' '};
        print(byteHeader, sizeof(byteHeader)+1);
        print(devPtr->device_name, DEVICE_NAME_LEN);
        printLn();
#endif
        
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
        if (fileSize >= deviceSize) 
            continue;
        
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
        fileBuffer[10] = devPtr->hardware_slot + '0';
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
    devicePart.block_size=0;
    
    for (uint8_t index=0; index < NUMBER_OF_PERIPHERALS; index++) {
        // Get the root directory from the storage device
        uint32_t deviceAddress = PERIPHERAL_ADDRESS_BEGIN + (index * PERIPHERAL_STRIDE);
        fsDeviceSetBase( deviceAddress );
        
        devicePart = fsDeviceOpen( 0x00000 );
        if (devicePart.block_size == 0) 
            continue;
        
        // Set the root
        rootDirectory = fsDeviceGetRootDirectory(devicePart);
        fsWorkingDirectorySetRoot( devicePart, rootDirectory);
        
        uint8_t consolePrompt[] = "x>";
        consolePrompt[0] = index + 'a';
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
        fsEnvironmentSetHomeDevice(deviceAddress);
        break;
    }
    
    // If no devices where found use temporary memory storage
    if (devicePart.block_size == 0) {
        fsDeviceSetBase(0x00000);
        devicePart = fsDeviceOpen(0x00000);
        
        uint8_t consolePrompt[] = "x>";
        ConsoleSetPrompt(consolePrompt, sizeof(consolePrompt));
    }
    
    // Load drivers
    
#ifndef _BOOT_SAFEMODE__
    
    // Locate drivers directory
    uint8_t driversDirName[] = "sys";
    uint32_t directoryAddress = fsDirectoryFindByName(devicePart, rootDirectory, driversDirName);
    
    if (directoryAddress == 0) 
        return;
    
    uint32_t numberOfFiles = fsDirectoryGetTotalSize(devicePart, directoryAddress);
    for (uint32_t f=0; f < numberOfFiles; f++) {
        FileHandle handle = fsDirectoryFindByIndex(devicePart, directoryAddress, f);
        
        // Get file size
        uint32_t fileSize = fsFileGetSize(devicePart, handle);
        if (fileSize >= deviceSize) 
            continue;
        
        uint8_t filename[FILE_NAME_LENGTH];
        fsFileGetName(devicePart, handle, filename);
        
        uint8_t driverBuffer[fileSize];
        
        // Load the file
        int32_t index = fsFileOpen(devicePart, handle);
        fsFileRead(devicePart, index, driverBuffer, fileSize);
        fsFileClose(index);
        
        // Check if the hardware exists on the bus
        uint8_t numberOfDevices = GetNumberOfDevices();
        for (uint8_t d=0; d < numberOfDevices; d++) {
            struct Device* devicePtr = GetDeviceByIndex(d);
            if (devicePtr->device_id == 0) 
                continue;
            uint8_t nameLength=0;
            for (unsigned int i=0; i < DEVICE_NAME_LEN; i++) {
                nameLength = i+1;
                if (devicePtr->device_name[i] == ' ') 
                    break;
            }
            
#ifdef _BOOT_DETAILS__
            
            // Check device name match
            if (StringCompare(devicePtr->device_name, nameLength-2, &driverBuffer[2], nameLength-2) == 0) {
                print(&driverBuffer[2], nameLength);
                printLn();
                
                // Load the driver into the kernel
                int8_t libState = LoadLibrary(devicePart, directoryAddress, filename);
                
                // Driver was not loaded correctly or is corrupted
                if (libState < 0) {
                    uint8_t driverError[] = "Error loading driver";
                    print(driverError, sizeof(driverError));
                    printLn();
                    break;
                }
                
            }
            
#endif
            
        }
        
    }
#endif
}

void KernelVectorTableInit(void) {
    isr_bus.read_waitstate  = 1;
    
    for (uint8_t i=0; i < HARDWARE_INTERRUPT_TABLE_SIZE; i++) 
        hardware_interrupt_table[i] = nullfunction;
}

void SetInterruptVectorCallback(uint8_t index, void(*callbackService)()) {
    hardware_interrupt_table[index] = callbackService;
}

struct Mutex isr_mux;

void _ISR_hardware_service_routine(void) {
    uint8_t mask = 0;
    MutexLock(&isr_mux);
    
    // Get the interrupt vector to determine what triggered it
    bus_read_byte(&isr_bus, HARDWARE_INTERRUPT_VECTOR, &mask);
    
    while (mask != 0) {
        // Get the least-significant set bit index
        uint8_t bit = __builtin_ctz(mask);
        hardware_interrupt_table[bit]();
        
        // clear it
        mask &= (uint8_t)(mask - 1);
    }
    
    MutexUnlock(&isr_mux);
}
