#include <kernel/fs/fs.h>
#include <kernel/list.h>
#include <kernel/cstring.h>

struct Node* MountPointTableHead = NULL;

struct MountPoint {
    
    // Address of the storage device
    uint32_t device_address;
    
    // Letter representing the storage device
    uint8_t letter;
};

void fsMount(uint32_t device_address, uint8_t letter) {
    uppercase(&letter);
    struct MountPoint* mountPoint = (struct MountPoint*)malloc(sizeof(struct MountPoint));
    mountPoint->device_address = device_address;
    mountPoint->letter = letter;
    
    ListAddNode(&MountPointTableHead, (void*)mountPoint);
    return;
}

void fsUnmount(uint8_t letter) {
    uppercase(&letter);
    uint32_t numberOfMountPoints = ListGetSize(MountPointTableHead);
    for (uint16_t i=0; i < numberOfMountPoints; i++) {
        struct Node* nodePtr = ListGetNode(MountPointTableHead, i);
        struct MountPoint* mountPoint = nodePtr->data;
        if (mountPoint->letter == letter) 
            ListRemoveNode(&MountPointTableHead, mountPoint);
    }
    return;
}

uint32_t fsMountGetNumberOfPoints(uint8_t letter) {
    return ListGetSize(MountPointTableHead);
}

struct MountPoint* fsMountGetMountPointByIndex(uint16_t index) {
    return (struct MountPoint*)ListGetNode(MountPointTableHead, index)->data;
}
