#include <kernel/fs/fs.h>

void fs_write_byte(uint32_t address, uint8_t data);
void fs_read_byte(uint32_t address, uint8_t* data);

uint32_t fileList[MAX_OPEN_FILES];
uint32_t fileSize[MAX_OPEN_FILES];

FileHandle fsFileCreate(struct Partition part, uint8_t* filename, uint32_t size) {
    FileHandle handle = fsAllocate(part, size);
    if (handle == 0) 
        return 0;
    
    fsFileSetName(part, handle, filename);
    
    uint8_t attributes[] = " rw ";
    fsFileSetAttributes(part, handle, attributes);
    
    // Extent chain
    fsFileSetNextAddress(part, handle, 0);
    fsFileSetParentAddress(part, handle, 0);
    
    // Set default flag
    fs_write_byte(part.block_address + handle + FILE_OFFSET_FLAG, 0x00);
    return handle;
}

FileHandle fsFileDelete(struct Partition part, FileHandle handle) {
    
    fsFree(part, handle);
    
    return handle;
}

FileHandle fsFileExtentCreate(struct Partition part, uint32_t size, uint32_t parentPtr, uint32_t nextPtr) {
    uint8_t filename[] = "EXTENT";
    FileHandle handle = fsFileCreate(part, filename, size);
    
    // Set number of entries in this directory extent
    fsDirectorySetReferenceCount(part, handle, 0);
    // Mark as an extent 'E'
    fsFileSetAttributes(part, handle, (uint8_t*)" rwE");
    
    // Set the extent chain
    fsFileSetNextAddress(part, handle, nextPtr);
    fsFileSetParentAddress(part, handle, parentPtr);
    
    return handle;
}


File fsFileOpen(struct Partition part, FileHandle handle) {
    uint8_t index = 0;
    for (uint8_t i=0; i <= MAX_OPEN_FILES; i++) {
        if (index == MAX_OPEN_FILES) 
            return -1;
        if (fileList[i] != 0) 
            continue;
        index = i;
        break;
    }
    
    // Check file header byte
    uint8_t headerByte;
    fs_read_byte(part.block_address + handle, &headerByte);
    
    if (headerByte == SECTOR_FREE) 
        return -1;
    
    // Get file size
    fileSize[index] = fsFileGetSize(part, handle);
    fileList[index] = part.block_address + handle;
    
    return index;
}

uint8_t fsFileClose(File index) {
    if (index < 0 || index > MAX_OPEN_FILES) 
        return 0;
    // Check already closed
    if (fileList[index] == 0) 
        return 0;
    fileList[index] = 0;
    fileSize[index] = 0;
    return 1;
}

uint8_t fsFileWrite(struct Partition part, File index, uint8_t* buffer, uint32_t size) {
    if (index < 0 || index > MAX_OPEN_FILES) 
        return 0;
    if (fileList[index] == 0) 
        return 0;
    if ((size-1) >= fileSize[index]+2) 
        return 0;
    
    uint32_t sector = 0;
    uint32_t sectorCounter = 0;
    
    for (uint32_t i=0; i < (size-1); i++) {
        
        // Skip the sector marker byte
        if (sectorCounter == (part.sector_size - 1)) {
            sectorCounter=0;
            sector++;
        }
        
        uint32_t positionOffset = part.sector_size + sector + 1;
        
        fs_write_byte(fileList[index] + positionOffset, buffer[i]);
        
        sectorCounter++;
        sector++;
    }
    
    return 1;
}

uint8_t fsFileRead(struct Partition part, File index, uint8_t* buffer, uint32_t size) {
    if (index < 0 || index > MAX_OPEN_FILES) 
        return 0;
    if (fileList[index] == 0) 
        return 0;
    if ((size-1) >= fileSize[index]+2) 
        return 0;
    
    uint32_t sector = 0;
    uint32_t sectorCounter = 0;
    
    for (uint32_t i=0; i < (size-1); i++) {
        
        // Skip the sector marker byte
        if (sectorCounter == (part.sector_size - 1)) {
            sectorCounter=0;
            sector++;
        }
        
        uint32_t positionOffset = part.sector_size + sector + 1;
        
        fs_read_byte(fileList[index] + positionOffset, &buffer[i]);
        
        sectorCounter++;
        sector++;
    }
    return 1;
}


void fsFileGetName(struct Partition part, FileHandle handle, uint8_t* name) {
    for (uint8_t i=0; i < FILE_NAME_LENGTH; i++) {
        fs_read_byte(part.block_address + handle + i + FILE_OFFSET_NAME, &name[i]);
        if (name[i] == '\0') {
            name[i] = ' ';
            break;
        }
    }
    return;
}

void fsFileSetName(struct Partition part, FileHandle handle, uint8_t* name) {
    // Clean up the file name
    uint8_t filename[FILE_NAME_LENGTH];
    for (uint32_t i=0; i < FILE_NAME_LENGTH; i++) 
        filename[i] = ' ';
    for (uint32_t i=0; i < FILE_NAME_LENGTH; i++) {
        if (name[i] == '\0') 
            break;
        filename[i] = name[i];
    }
    for (uint32_t i=0; i < FILE_NAME_LENGTH; i++) 
        fs_write_byte(part.block_address + handle + i + FILE_OFFSET_NAME, filename[i]);
    return;
}

uint32_t fsFileGetSize(struct Partition part, FileHandle handle) {
    uint8_t sizeBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + handle + i + FILE_OFFSET_SIZE, &sizeBytes[i]);
    return *((uint32_t*)&sizeBytes[0]);
}

void fsFileSetAttributes(struct Partition part, FileHandle handle, uint8_t* attributes) {
    for (uint32_t i=0; i < 4; i++) 
        fs_write_byte(part.block_address + handle + i + FILE_OFFSET_ATTRIBUTES, attributes[i]);
    return;
}

void fsFileGetAttributes(struct Partition part, FileHandle handle, uint8_t* attributes) {
    for (uint32_t i=0; i < 4; i++) 
         fs_read_byte(part.block_address + handle + i + FILE_OFFSET_ATTRIBUTES, &attributes[i]);
    return;
}


void fsFileSetFlag(struct Partition part, FileHandle handle, uint8_t index, uint8_t state) {
    uint8_t flag;
    fs_read_byte(part.block_address + handle + FILE_OFFSET_FLAG, &flag);
    if (state == 1) {
        flag |= (1 << index);
    } else {
        flag &= ~(1 << index);
    }
    fs_write_byte(part.block_address + handle + FILE_OFFSET_FLAG, flag);
    return;
}

uint8_t fsFileGetFlag(struct Partition part, FileHandle handle, uint8_t index) {
    uint8_t flag;
    fs_read_byte(part.block_address + handle + FILE_OFFSET_FLAG, &flag);
    return (flag >> index) & 1;
}

void fsFileSetParentAddress(struct Partition part, FileHandle handle, uint32_t parent) {
    uint8_t ptrBytes[4];
    *((uint32_t*)&ptrBytes[0]) = parent;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part.block_address + handle + i + FILE_OFFSET_PARENT, ptrBytes[i]);
    return;
}

uint32_t fsFileGetParentAddress(struct Partition part, FileHandle handle) {
    uint8_t ptrBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + handle + i + FILE_OFFSET_PARENT, &ptrBytes[i]);
    return *((uint32_t*)&ptrBytes[0]);
}


void fsFileSetNextAddress(struct Partition part, FileHandle handle, uint32_t next) {
    uint8_t ptrBytes[4];
    *((uint32_t*)&ptrBytes[0]) = next;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part.block_address + handle + i + FILE_OFFSET_NEXT, ptrBytes[i]);
    return;
}

uint32_t fsFileGetNextAddress(struct Partition part, FileHandle handle) {
    uint8_t ptrBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + handle + i + FILE_OFFSET_NEXT, &ptrBytes[i]);
    return *((uint32_t*)&ptrBytes[0]);
}
