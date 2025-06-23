#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>

#define MAX_DIRECTORY_DEPTH  8

uint8_t current_directory_index;
DirectoryHandle fs_working_directory[MAX_DIRECTORY_DEPTH];

void fsWorkingDirectorySetRoot(DirectoryHandle handle) {
    fs_working_directory[0] = handle;
    current_directory_index = 0;
    return;
}

DirectoryHandle fsWorkingDirectoryGetRoot(void) {
    return fs_working_directory[0];
}

void fsWorkingDirectoryChange(DirectoryHandle handle) {
    current_directory_index++;
    fs_working_directory[current_directory_index] = handle;
    return;
}

void fsWorkingDirectorySetParent(void) {
    if (current_directory_index == 0) 
        return;
    current_directory_index--;
    return;
}

DirectoryHandle fsWorkingDirectoryGetCurrent(void) {
    return fs_working_directory[current_directory_index];
}

uint8_t fsWorkingDirectoryGetIndex(void) {
    return current_directory_index;
}

