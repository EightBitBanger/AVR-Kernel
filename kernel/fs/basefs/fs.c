#include <kernel/fs/fs.h>
#include <string.h>
void fs_write_byte(uint32_t address, uint8_t data);
void fs_read_byte(uint32_t address, uint8_t* data);


void fsParsePath(struct Partition part, uint8_t* path) {
    DirectoryHandle rootDirectory = fsDeviceGetRootDirectory(part);
    
    const char* delimiter = "\\";
    char* token = strtok((char*)path, delimiter);
    while (token != NULL) {
        
        uint32_t subDirHandle = fsFindDirectory(part, rootDirectory, (uint8_t*)token);
        
        fsWorkingDirectoryChange(subDirHandle);
        
        token = strtok(NULL, delimiter);
    }
    return;
}


uint32_t fsAllocate(struct Partition part, uint32_t size) {
    uint32_t address = 0;
    uint32_t numberOfSectors = (size / (part.sector_size - 1)) + 1;
    
    // Find available space
    for (uint32_t i=0; i < part.block_size; i++) {
        uint8_t buffer;
        fs_read_byte(part.block_address + (i * part.sector_size), &buffer);
        address = 0;
        
        if (buffer != SECTOR_FREE) 
            continue;
        
        // Check if the allocation will fit
        for (uint32_t a=0; a <= numberOfSectors; a++) {
            fs_read_byte(part.block_address + ((a + i) * part.sector_size), &buffer);
            
            if (buffer != SECTOR_FREE) {
                address = 1;
                break;
            }
            if (a == numberOfSectors) 
                break;
            continue;
        }
        
        if (address == 1) 
            continue;
        
        address = i * part.sector_size;
        break;
    }
    
    if (address == 0) 
        return 0;
    
    // Mark following sectors as taken
    fs_write_byte(part.block_address + address, SECTOR_HEADER);
    for (uint32_t i=0; i < numberOfSectors; i++) 
        fs_write_byte(part.block_address + address + (i * part.sector_size) + part.sector_size, SECTOR_USED);
    
    // Write allocation size
    uint8_t sizeBytes[4];
    *((uint32_t*)&sizeBytes[0]) = size;
    
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part.block_address + address + i + FILE_OFFSET_SIZE, sizeBytes[i]);
    
    return part.block_address + address;
}

void fsFree(struct Partition part, uint32_t address) {
    // Get allocation size
    uint32_t sizeTotal = fsFileGetSize(part, address);
    uint32_t numberOfSectors = (sizeTotal / (part.sector_size - 1)) + 2;
    
    // Mark sectors as free
    for (uint32_t i=0; i < numberOfSectors; i++) 
        fs_write_byte(address + (i * part.sector_size), SECTOR_FREE);
    return;
}
