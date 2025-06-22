#include <kernel/fs/fs.h>

uint32_t fsFindFile(struct Partition part, DirectoryHandle handle, uint8_t* filename) {
    uint32_t fileHandle = fsDirectoryFindByName(part, handle, filename);
    if (fileHandle == 0) 
        return 0;
    uint8_t attributes[4];
    fsFileGetAttributes(part, fileHandle, attributes);
    if (attributes[3] == 'd') 
        return 0;
    return fileHandle;
}

uint32_t fsFindDirectory(struct Partition part, DirectoryHandle handle, uint8_t* filename) {
    uint32_t directoryHandle = fsDirectoryFindByName(part, handle, filename);
    if (directoryHandle == 0) 
        return 0;
    uint8_t attributes[4];
    fsFileGetAttributes(part, directoryHandle, attributes);
    if (attributes[3] != 'd') 
        return 0;
    
    return directoryHandle;
}
