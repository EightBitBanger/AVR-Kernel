#include <kernel/vfs.h>
#include <kernel/fs/fs.h>

int32_t vfsOpen(uint8_t* filename, uint8_t nameLength) {
    /*
    uint32_t fileAddress = fsFileExists(filename, nameLength);
    // File does not exist. Create a new file
    if (fileAddress == 0) 
        fileAddress = fsFileCreate(filename, nameLength, 32);
    if (fileAddress == 0) 
        return -1;
    return fsFileOpen(fileAddress);
    */
    return 0;
}

uint8_t vfsClose(int32_t index) {
    
    //return fsFileClose(index);
    return 0;
}

uint8_t vfsRead(int32_t index, uint8_t buffer, uint32_t size) {
    //return fsFileRead(index, &buffer, size);
    return 0;
}

uint8_t vfsWrite(int32_t index, uint8_t buffer, uint32_t size) {
    //return fsFileWrite(index, &buffer, size);
    return 0;
}

uint8_t vfsChdir(uint8_t* directoryName, uint8_t nameLength) {
    //return vfsLookup(directoryName, nameLength);
    return 0;
}

uint8_t vfsMkdir(uint8_t* filename, uint8_t nameLength) {
    /*
    uint32_t fileAddress = fsDirectoryExists(filename, nameLength);
    // File does not exist. Create a new file
    if (fileAddress == 0) 
        fileAddress = fsDirectoryCreate(filename, nameLength);
    if (fileAddress == 0) 
        return 0;
    */return 1;
}

uint8_t vfsRmdir(uint8_t* filename, uint8_t nameLength) {
    /*
    uint32_t fileAddress = fsDirectoryExists(filename, nameLength);
    if (fileAddress == 0) 
        return 0;
    return fsDirectoryDelete(filename, nameLength);
    */
    return 0;
}

uint8_t vfsLookup(uint8_t* path, uint8_t pathLength) {
    /*
    // Copy the path string to avoid modifying the original
    uint8_t* path_copy[128];
    strncpy((char*)path_copy, (char*)path, sizeof(path_copy) - 1);
    
    // Ensure null-termination
    path_copy[sizeof(path_copy) - 1] = (uint8_t*)'\0';
    
    // Explode the string by the delimiter
    uint8_t* token = (uint8_t*)strtok((char*)path_copy, "/");
    
    // Process each token (directory name)
    while (token != NULL) {
        
        if (fsWorkingDirectoryChange(token, strlen((char*)token)+1) != 0) 
            return 1;
        
        // Get the next token
        token = (uint8_t*)strtok(NULL, "/");
    }
    */
    return 0;
}

uint8_t vfsList(struct Partition part, DirectoryHandle handle) {
    uint8_t msgDir[] = "<dir>";
    
    if (fsDeviceCheck(part) == 0) 
        return 0;
    
    uint32_t directoryRefTotal = fsDirectoryGetTotalSize(part, handle);
    if (directoryRefTotal == 0) 
        return 0;
    
    for (uint32_t i=0; i < directoryRefTotal; i++) {
        uint32_t entryHandle = fsDirectoryFindByIndex(part, handle, i);
        
        // Print attributes
        uint8_t fileAttr[] = "    ";
        fsFileGetAttributes(part, entryHandle, fileAttr);
        
        // Check directory or file
        uint8_t isDirectory = 0;
        if (fileAttr[3] == 'd') 
            isDirectory = 1;
        
        print(fileAttr, 4);
        printSpace(1);
        
        // Print file name
        uint8_t filename[] = "          ";
        fsFileGetName(part, entryHandle, filename);
        print(filename, sizeof(filename));
        printSpace(1);
        
        // Print file size / dir
        if (isDirectory == 1) {
            print(msgDir, sizeof(msgDir));
        } else {
            uint32_t fileSize = fsFileGetSize(part, entryHandle);
            printInt(fileSize);
        }
        printLn();
    }
    return 0;
}
