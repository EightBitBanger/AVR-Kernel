#include <kernel/fs/fs.h>
#include <kernel/list.h>
#include <kernel/cstring.h>

struct Node* MountPointTableHead = NULL;

struct MountPoint {
    uint32_t device_address;   // Address of the storage device
    uint8_t letter;            // Letter representing the storage device
    
};

uint8_t fsMount(uint32_t device_address, uint8_t letter) {
    uppercase(&letter);
    struct MountPoint* mountPoint = (struct MountPoint*)malloc(sizeof(struct MountPoint));
    mountPoint->device_address = device_address;
    mountPoint->letter = letter;
    
    ListAddNode(&MountPointTableHead, (void*)mountPoint);
    return 1;
}

uint8_t fsUnmount(uint8_t letter) {
    uppercase(&letter);
    uint32_t numberOfMountPoints = ListGetSize(MountPointTableHead);
    for (uint16_t i=0; i < numberOfMountPoints; i++) {
        struct Node* nodePtr = ListGetNode(MountPointTableHead, i);
        struct MountPoint* mountPoint = nodePtr->data;
        if (mountPoint->letter == letter) {
            ListRemoveNode(&MountPointTableHead, mountPoint);
            return 1;
        }
    }
    return 0;
}

uint32_t fsMountGetNumberOfPoints(uint8_t letter) {
    return ListGetSize(MountPointTableHead);
}

struct MountPoint* fsMountGetMountPointByIndex(uint16_t index) {
    return (struct MountPoint*)ListGetNode(MountPointTableHead, index)->data;
}
