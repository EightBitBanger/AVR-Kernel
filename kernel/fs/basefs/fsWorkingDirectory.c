#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>

#define MAX_DIRECTORY_DEPTH  8

uint8_t current_directory_index;
DirectoryHandle fs_working_directory[MAX_DIRECTORY_DEPTH];
uint32_t fs_working_device[MAX_DIRECTORY_DEPTH];


void fsWorkingDirectorySetRoot(struct Partition part, DirectoryHandle handle) {
    fs_working_directory[0] = handle;
    fs_working_device[0] = part.block_address;
    current_directory_index = 0;
    return;
}

DirectoryHandle fsWorkingDirectoryGetRoot(void) {
    return fs_working_directory[0];
}

void fsWorkingDirectoryChange(struct Partition part, DirectoryHandle handle) {
    current_directory_index++;
    
    // Check if the target directory is a mounted directory
    fsDeviceSetCurrent(part.block_address);
    uint8_t attributes[4];
    fsFileGetAttributes(part, handle, attributes);
    if (attributes[0] == 'm' && 
        attributes[3] == 'd') {
        
        // Device address block
        uint32_t block_address = fsFileGetNextAddress(part, handle);
        fsDeviceSetCurrent(block_address);
        
        // Target directory
        DirectoryHandle mountedHandle = fsFileGetParentAddress(part, handle);
        
        // Add the mounted directory to the directory list
        fs_working_directory[current_directory_index] = mountedHandle;
        fs_working_device[current_directory_index] = block_address;
        return;
    }
    
    // Add the directory to the directory list
    fs_working_directory[current_directory_index] = handle;
    fs_working_device[current_directory_index] = part.block_address;
    return;
}

uint8_t fsWorkingDirectorySetParent(void) {
    if (current_directory_index == 0) 
        return 0;
    current_directory_index--;
    return current_directory_index;
}

DirectoryHandle fsWorkingDirectoryGetCurrent(void) {
    return fs_working_directory[current_directory_index];
}

uint8_t fsWorkingDirectoryGetIndex(void) {
    return current_directory_index;
}

