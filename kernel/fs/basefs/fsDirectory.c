#include <kernel/fs/fs.h>
#include <stdio.h>
#include <string.h>

void fs_write_byte(uint32_t address, uint8_t data);
void fs_read_byte(uint32_t address, uint8_t* data);


DirectoryHandle fsDirectoryCreate(struct Partition part, uint8_t* filename) {
    FileHandle handle = fsFileCreate(part, filename, 30);
    
    // Set number of entries in this directory
    fsDirectorySetReferenceCount(part, handle, 0);
    // Mark as a directory 'd'
    fsFileSetAttributes(part, handle, (uint8_t*)" rwd");
    
    return handle;
}

DirectoryHandle fsDirectoryDelete(struct Partition part, DirectoryHandle handle) {
    
    // TODO Purge any files / directories currently in this directory
    
    fsFileDelete(part, handle);
    
    return handle;
}

DirectoryHandle fsDirectoryExtentCreate(struct Partition part, uint32_t parentPtr, uint32_t nextPtr) {
    uint8_t filename[] = "EXTENT";
    FileHandle handle = fsFileCreate(part, filename, 30);
    
    // Set number of entries in this directory extent
    fsDirectorySetReferenceCount(part, handle, 0);
    // Mark as an extent 'E'
    fsFileSetAttributes(part, handle, (uint8_t*)" rwE");
    
    // Set the extent chain
    fsFileSetNextAddress(part, handle, nextPtr);
    fsFileSetParentAddress(part, handle, parentPtr);
    
    return handle;
}


void fsDirectorySetReferenceCount(struct Partition part, DirectoryHandle handle, uint32_t count) {
    uint8_t sizeBytes[4];
    *((uint32_t*)&sizeBytes[0]) = count;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part.block_address + handle + i + FILE_OFFSET_REF_COUNT, sizeBytes[i]);
    return;
}

uint32_t fsDirectoryGetReferenceCount(struct Partition part, DirectoryHandle handle) {
    uint8_t sizeBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + handle + i + FILE_OFFSET_REF_COUNT, &sizeBytes[i]);
    return *((uint32_t*)&sizeBytes[0]);
}


uint8_t fsDirectoryAddFile(struct Partition part, DirectoryHandle handle, uint32_t file) {
    uint32_t refCount = fsDirectoryGetReferenceCount(part, handle);
    
    // Check if the directory is full
    if (((refCount+1) * 4) >= part.sector_size) {
        // Get the first  extent
        DirectoryHandle extent = fsFileGetNextAddress(part, handle);
        
        // If no extent was found, generate a new one
        if (extent == 0) {
            extent = fsDirectoryExtentCreate(part, handle, extent);
            fsFileSetNextAddress(part, handle, extent);
        } else {
            
            // Check extents recursively
            while (extent != 0) {
                
                // Check if the next extent is full
                uint32_t extentRefCount = fsDirectoryGetReferenceCount(part, extent);
                if (((extentRefCount+2) * 4) >= part.sector_size) {
                    
                    // Check end of extent list
                    DirectoryHandle nextExtent = fsFileGetNextAddress(part, extent);
                    if (nextExtent == 0) {
                        
                        // Generate a new extent
                        DirectoryHandle newExtent = fsDirectoryExtentCreate(part, extent, 0);
                        fsFileSetNextAddress(part, extent, newExtent);
                        break;
                    }
                    
                } else {
                    // Room exists in this extent
                    break;
                }
                
                // Get next extent
                extent = fsFileGetNextAddress(part, extent);
            }
            
        }
        
        // Set the target extent
        handle = extent;
        refCount = fsDirectoryGetReferenceCount(part, handle);
    }
    
    fsDirectorySetReferenceCount(part, handle, refCount+1);
    
    File index = fsFileOpen(part, handle);
    
    // Read directory references
    uint32_t fileSize = fsFileGetSize(part, handle);
    uint8_t buffer[fileSize + part.sector_size];
    fsFileRead(part, index, buffer, fileSize);
    
    // Add new reference address
    uint8_t refBytes[4];
    *((uint32_t*)&refBytes[0]) = file;
    for (uint8_t i=0; i < 4; i++) 
        buffer[(refCount * 4) + i] = refBytes[i];
    
    // Write back references
    fsFileWrite(part, index, buffer, fileSize);
    fsFileClose(index);
    
    return 1;
}

uint32_t fsDirectoryGetTotalSize(struct Partition part, DirectoryHandle handle) {
    uint32_t total = 0;
    while (1) {
        total += fsDirectoryGetReferenceCount(part, handle);
        
        handle = fsFileGetNextAddress(part, handle);
        if (handle == 0) 
            break;
    }
    return total;
}

uint32_t fsDirectoryFindByIndex(struct Partition part, DirectoryHandle handle, uint32_t index) {
    uint8_t buffer[64];
    uint32_t counter = 0;
    
    while (1) {
        uint32_t refCount = fsDirectoryGetReferenceCount(part, handle);
        
        File fileIndex = fsFileOpen(part, handle);
        uint32_t directorySize = fsFileGetSize(part, handle);
        
        fsFileRead(part, fileIndex, buffer, directorySize);
        
        // Check the list of files in this extent
        for (uint32_t i=0; i < refCount; i++) {
            // Check index found
            if (counter == index) {
                
                uint8_t ptrBytes[4];
                for (uint8_t a=0; a < 4; a++) 
                    ptrBytes[a] = buffer[ (i * 4) + a ];
                
                uint32_t fileHandle = *((uint32_t*)&ptrBytes[0]);
                
                fsFileClose(fileIndex);
                return fileHandle;
            }
            
            counter++;
        }
        fsFileClose(fileIndex);
        
        handle = fsFileGetNextAddress(part, handle);
        if (handle == 0) 
            break;
    }
    
    return 0;
}

uint32_t fsDirectoryFindByName(struct Partition part, DirectoryHandle handle, uint8_t* filename) {
    uint8_t buffer[64];
    
    // Cleanse the source file name
    uint8_t sourceFilename[] = "          ";
    for (uint8_t i=0; i < 10; i++) {
        if (filename[i] == '\0') 
            break;
        sourceFilename[i] = filename[i];
    }
    
    while (1) {
        uint32_t refCount = fsDirectoryGetReferenceCount(part, handle);
        
        File index = fsFileOpen(part, handle);
        uint32_t directorySize = fsFileGetSize(part, handle);
        
        fsFileRead(part, index, buffer, directorySize);
        
        // Check the list of files in this extent
        for (uint32_t i=0; i < refCount; i++) {
            uint8_t ptrBytes[4];
            for (uint8_t a=0; a < 4; a++) 
                ptrBytes[a] = buffer[ (i * 4) + a ];
            
            uint32_t fileHandle = *((uint32_t*)&ptrBytes[0]);
            
            uint8_t name[] = "          ";
            fsFileGetName(part, fileHandle, name);
            
            if (strcmp((char*)name, (char*)sourceFilename) != 0) 
                continue;
            
            fsFileClose(index);
            return fileHandle;
        }
        fsFileClose(index);
        
        handle = fsFileGetNextAddress(part, handle);
        if (handle == 0) 
            break;
    }
    
    return 0;
}


uint8_t fsDirectoryRemoveFile(struct Partition part, DirectoryHandle handle, uint32_t file) {
    uint8_t buffer[64];
    uint8_t extentCount = 0;
    
    while (1) {
        uint32_t refCount = fsDirectoryGetReferenceCount(part, handle);
        
        File index = fsFileOpen(part, handle);
        uint32_t directorySize = fsFileGetSize(part, handle);
        
        fsFileRead(part, index, buffer, directorySize);
        
        for (uint32_t i=0; i < refCount; i++) {
            uint8_t ptrBytes[4];
            for (uint8_t a=0; a < 4; a++) 
                ptrBytes[a] = buffer[ (i * 4) + a ];
            
            uint32_t fileHandle = *((uint32_t*)&ptrBytes[0]);
            if (file != fileHandle) 
                continue;
            
            fsFileDelete(part, file);
            
            // Replace the entry with the last entry
            for (uint8_t a=0; a < 4; a++) {
                buffer[ (i * 4) + a ] = buffer[ ((refCount-1) * 4) + a ];
                buffer[ ((refCount-1) * 4) + a ] = ' ';
            }
            fsFileWrite(part, index, buffer, directorySize);
            
            fsDirectorySetReferenceCount(part, handle, refCount-1);
            fsFileClose(index);
            
            // Check to delete the extent if its empty
            if (refCount <= 1 && extentCount > 0) {
                
                // Bypass this link in the list
                uint32_t next   = fsFileGetNextAddress(part, handle);
                uint32_t parent = fsFileGetParentAddress(part, handle);
                
                fsFileSetNextAddress(part, parent, next);
                fsFileSetParentAddress(part, next, parent);
                
                fsFileDelete(part, handle);
            }
            
            return 1;
        }
        fsFileClose(index);
        
        handle = fsFileGetNextAddress(part, handle);
        extentCount++;
        if (handle == 0) 
            break;
    }
    
    return 0;
}

