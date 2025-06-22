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
    struct Partition part = fsDeviceOpen(0x00000000);
    fsDeviceFormat(&part, 0, 32768, 32);
    DirectoryHandle rootDirectory = fsDeviceGetRootDirectory(part);
    
    fsWorkingDirectorySetRoot(rootDirectory);
    
    DirectoryHandle binDirectoryHandle;
    
    {
    uint8_t filename[] = "bin";
    binDirectoryHandle = fsDirectoryCreate(part, filename);
    fsDirectoryAddFile(part, rootDirectory, binDirectoryHandle);
    }
    {
    uint8_t filename[] = "sys";
    DirectoryHandle directoryHandle = fsDirectoryCreate(part, filename);
    fsDirectoryAddFile(part, rootDirectory, directoryHandle);
    }
    
    {
    uint8_t filename[] = "file";
    FileHandle fileHandle = fsFileCreate(part, filename, 128);
    fsDirectoryAddFile(part, rootDirectory, fileHandle);
    }
    {
    uint8_t filename[] = "test";
    FileHandle fileHandle = fsFileCreate(part, filename, 40);
    fsDirectoryAddFile(part, rootDirectory, fileHandle);
    }
    
    {
    uint8_t filename[] = "prog";
    FileHandle fileHandle = fsFileCreate(part, filename, 40);
    fsDirectoryAddFile(part, binDirectoryHandle, fileHandle);
    }
    
    {
    uint8_t filename[] = "wtf";
    FileHandle fileHandle = fsFileCreate(part, filename, 40);
    fsDirectoryAddFile(part, binDirectoryHandle, fileHandle);
    }
    
    
    
    
    
    /*
    
    VirtualAccessSetMode(VIRTUAL_ACCESS_MODE_KERNEL);
    
    // Initiate console prompt
    uint8_t prompt[] = {fsDeviceGetRootLetter(), '>'};
    
    ConsoleSetPrompt(prompt, sizeof(prompt));
    
    // Initiate virtual file system
    vfs.open = vfsOpen;
    vfs.read = vfsRead;
    vfs.write = vfsWrite;
    vfs.close = vfsClose;
    vfs.mkdir = vfsMkdir;
    vfs.rmdir = vfsRmdir;
    
    // Initiate memory storage
    fsDeviceSetRootLetter('x');
    fsFormat(0, FORMAT_CAPACITY_32K);
    
    fsWorkingDirectoryClear();
    
    
    // Create system directories
    
    uint8_t dirNameDev[] = "dev";
    fsDirectoryCreate(dirNameDev, sizeof(dirNameDev));
    uint8_t dirNameProc[] = "proc";
    dirProcAddress = fsDirectoryCreate(dirNameProc, sizeof(dirNameProc));
    
    // Register devices on the bus
    
    fsWorkingDirectoryChange(dirNameDev, sizeof(dirNameDev));
    
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
        uint32_t fileAddress = fsFileCreate(devPtr->device_name, length, 40);
        
        struct FSAttribute attrib = {'s','r','w',' '};
        fsFileSetAttributes(fileAddress, &attrib);
        
        // Initiate file
        uint32_t fileSize = fsFileGetSize(fileAddress);
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
        
        int32_t fileIndex = fsFileOpen(fileAddress);
        fsFileWrite(fileIndex, fileBuffer, fileSize);
        fsFileClose(fileIndex);
        
        continue;
    }
    
    fsWorkingDirectoryClear();
    
    
    // Find an active storage device
    uint8_t promptDevLetter[] = " >";
    uint8_t selectedDevice = 255;
    
    for (uint8_t i=0; i < NUMBER_OF_PERIPHERALS; i++) {
        
        uint8_t currentDevice = i + 'a';
        
        fsDeviceSetRootLetter(currentDevice);
        
        if (fsDeviceCheckReady() == 0) 
            continue;
        
        selectedDevice = currentDevice;
        
        // Setup environment
        
        // Primary root device
        EnvironmentSetHomeChar( selectedDevice );
        
        // Executable search path
        uint8_t binPath[] = "bin";
        EnvironmentSetPathBin(binPath, sizeof(binPath));
        
        break;
    }
    
    fsDeviceSetRootLetter('x');
    fsWorkingDirectoryClear();
    
    // Set prompt to an active storage device
    
    if (selectedDevice == 255) {
        
        fsDeviceSetRootLetter('x');
        
        promptDevLetter[0] = 'X';
        
        ConsoleSetPrompt(promptDevLetter, sizeof(promptDevLetter));
        
    } else {
        
        fsDeviceSetRootLetter(selectedDevice);
        uppercase(&selectedDevice);
        
        promptDevLetter[0] = selectedDevice;
        
        ConsoleSetPrompt(promptDevLetter, sizeof(promptDevLetter));
        
    }
    
    fsWorkingDirectoryClear();
    
    
    
    
    //
    // Load drivers
    
#ifndef _BOOT_SAFEMODE__
    
    uint8_t driversDirName[] = "sys";
    
    //
    // Locate drivers directory
    //
    
    uint32_t directoryAddress = fsDirectoryExists(driversDirName, sizeof(driversDirName)-1);
    
    // Directory does not exist
    if (directoryAddress == 0) 
        return;
    
    fsWorkingDirectoryChange(driversDirName, sizeof(driversDirName));
    
    uint32_t numberOfFiles = fsWorkingDirectoryGetFileCount();
    
    for (uint32_t f=0; f < numberOfFiles; f++) {
        
        uint32_t fileAddress = fsWorkingDirectoryFind(f);
        
        if (fileAddress == 0) 
            continue;
        
        uint32_t fileSize = fsFileGetSize(fileAddress);
        uint8_t driverBuffer[fileSize];
        
        int32_t index = fsFileOpen(fileAddress);
        fsFileRead(index, driverBuffer, fileSize);
        fsFileClose(index);
        
        uint8_t driverFilename[FILE_NAME_LENGTH];
        uint8_t driverFilenameLength=0;
        
        for (uint32_t n=0; n < FILE_NAME_LENGTH; n++) {
            fs_read_byte(fileAddress + FILE_OFFSET_NAME + n, &driverFilename[n]);
            if (driverFilename[n] == ' ') {
                driverFilenameLength = n;
                break;
            }
            
        }
        
        uint32_t checkFileAddress = fsFileExists(driverFilename, driverFilenameLength);
        
        // Check if the device does not exist on the bus
        // The driver can be skipped
        
        int32_t driverFileIndex = fsFileOpen(fileAddress);
        uint32_t driverFileSz = fsFileGetSize(fileAddress);
        
        uint8_t fileBuffer[driverFileSz];
        fsFileRead(driverFileIndex, fileBuffer, driverFileSz);
        fsFileClose(driverFileIndex);
        
        uint8_t numberOfDevices = GetNumberOfDevices();
        
        // Check if the device exists
        uint8_t found = 0;
        for (uint8_t d=0; d < numberOfDevices; d++) {
            struct Device* devicePtr = GetDeviceByIndex(d);
            
            if (StringCompare(devicePtr->device_name, DEVICE_NAME_LEN, &fileBuffer[2], DEVICE_NAME_LEN) == 1) {
                found = 1;
                break;
            }
            continue;
        }
        if (found == 0) 
            continue;
        
        //
        // Load the driver
        
        int8_t libState = -32;
        if (checkFileAddress != 0) 
            libState = LoadLibrary(driverFilename, driverFilenameLength);
        
#ifdef _BOOT_DETAILS__
        
        // Driver is linked to a device on the bus
        if (libState == 4) {
            // Print the characters of the driver name
            for (uint32_t n=0; n < FILE_NAME_LENGTH; n++) {
                printChar( driverBuffer[n + 2] );
                if (driverBuffer[n + 2] == ' ') 
                    break;
            }
            
            printLn();
            continue;
        }
        
        // Driver was not loaded correctly or is corrupted
        if ((libState < 0) & (libState != -32)) {
            // Driver failed to load
            uint8_t msgFailedToLoad[] = "... failed";
            print(msgFailedToLoad, sizeof(msgFailedToLoad));
            
            printLn();
        }
        
#endif
        
        continue;
    }
    
#endif
    
    fsWorkingDirectoryClear();
    */
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

void _ISR_hardware_service_routine(void) {
    
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
    
    return;
}


void nullfunction(void) {return;}
