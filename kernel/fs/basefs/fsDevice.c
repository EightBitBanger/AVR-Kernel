#include <kernel/fs/fs.h>
#include <kernel/bus/bus.h>
#include <string.h>

const uint8_t DeviceHeaderString[] = {0x13, 'f','s',' ',' ',' ',' ',' ',' ',' ',' '};

struct Bus fs_bus;

uint32_t current_partition;
uint32_t base_address;

extern uint32_t device_home_address;

void (*fsWriteSectorByte)(struct Bus*, uint32_t, uint8_t);
void (*fsReadSectorByte)(struct Bus*, uint32_t, uint8_t*);


void fs_write_byte(uint32_t address, uint8_t data) {
    fsWriteSectorByte(&fs_bus, base_address + address, data);
}

void fs_read_byte(uint32_t address, uint8_t* data) {
    fsReadSectorByte(&fs_bus, base_address + address, data);
}

void fsDeviceSetType(uint8_t device_type) {
    if (device_type == FS_DEVICE_TYPE_EEPROM) {
        fsWriteSectorByte = bus_write_byte_eeprom;
        fsReadSectorByte = bus_read_memory;
        return;
    }
    
    // Default
    fsWriteSectorByte = bus_write_memory;
    fsReadSectorByte = bus_read_memory;
    return;
}


void fsInit(void) {
    
    fs_bus.bus_type = 0;
    fs_bus.read_waitstate = 1;
    fs_bus.write_waitstate = 0;
    
    current_partition = 0x00000000;
    base_address      = 0x00000000;
    
    fsWriteSectorByte = bus_write_memory;
    fsReadSectorByte = bus_read_memory;
    
    device_home_address = 0;
    
    return;
}

struct Partition fsDeviceOpen(uint32_t device_address) {
    struct Partition part;
    part.block_address = device_address;
    
    // Get device ID bytes
    uint8_t headerID[10];
    for (uint8_t i=0; i < sizeof(headerID); i++) 
        fs_read_byte(part.block_address + i, &headerID[i]);
    
    part.block_size = fsDeviceGetSize(part);
    part.sector_size  = fsDeviceGetSectorSize(part);
    
    if (headerID[0] != 0x13 || headerID[1] != 'f' || headerID[2] != 's' ||
        part.block_size == 0 || part.sector_size == 0) {
        
        part.block_size = 0;
        part.sector_count = 0;
        part.sector_size = 0;
        return part;
    }
    
    // Check device type
    uint8_t typeIdentifier;
    fs_read_byte(part.block_address + DEVICE_OFFSET_TYPE, &typeIdentifier);
    fsDeviceSetType(typeIdentifier);
    
    part.sector_count = part.block_size / part.sector_size;
    return part;
}

uint32_t fsDeviceGetSize(struct Partition part) {
    uint8_t sizeBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + i + DEVICE_OFFSET_CAPACITY, &sizeBytes[i]);
    return *((uint32_t*)&sizeBytes[0]);
}

uint32_t fsDeviceGetSectorSize(struct Partition part) {
    uint8_t sizeBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + i + DEVICE_OFFSET_SECT_SZ, &sizeBytes[i]);
    return *((uint32_t*)&sizeBytes[0]);
}

DirectoryHandle fsDeviceGetRootDirectory(struct Partition part) {
    uint8_t ptrBytes[4];
    for (uint8_t i=0; i < 4; i++) 
        fs_read_byte(part.block_address + i + DEVICE_OFFSET_ROOT, &ptrBytes[i]);
    return (*((uint32_t*)&ptrBytes[0]));
}

uint32_t fsDeviceGetCurrent(void) {
    return current_partition;
}

void fsDeviceSetCurrent(uint32_t address) {
    current_partition = address;
    return;
}

uint32_t fsDeviceGetBase(void) {
    return base_address;
}

void fsDeviceSetBase(uint32_t address) {
    base_address = address;
    return;
}

void fsDeviceFormat(struct Partition* part, uint32_t begin, uint32_t end, uint32_t sector_size, uint8_t device_type, uint8_t* device_name) {
    fsDeviceSetType(device_type);
    part->block_size = end - begin;
    part->sector_size = sector_size;
    part->sector_count = part->block_size / part->sector_size;
    
    // Mark sectors as empty
    for (uint32_t i=0; i < part->sector_count; i++) 
        fs_write_byte(part->block_address + (i * part->sector_size), SECTOR_FREE);
    
    fsDeviceConstructAllocationTable(part, device_type, device_name);
    return;
}

void fsDeviceFormatLow(struct Partition* part, uint32_t begin, uint32_t end, uint32_t sector_size, uint8_t device_type) {
    fsDeviceSetType(device_type);
    part->block_size = end - begin;
    part->sector_size = sector_size;
    part->sector_count = part->block_size / part->sector_size;
    
    for (uint32_t i=0; i < part->block_size; i++) 
        fs_write_byte(part->block_address + i, ' ');
    
    return;
}

uint8_t fsDeviceConstructAllocationTable(struct Partition* part, uint8_t device_type, uint8_t* device_name) {
    
    // Initiate device header and associated information
    for (uint32_t i=0; i < sizeof(DeviceHeaderString); i++) 
        fs_write_byte(part->block_address + i, DeviceHeaderString[i]);
    
    // Set device capacity
    uint8_t sizeBytes[4];
    *((uint32_t*)&sizeBytes[0]) = part->block_size;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part->block_address + i + DEVICE_OFFSET_CAPACITY, sizeBytes[i]);
    
    // Set the device type
    fs_write_byte(part->block_address + DEVICE_OFFSET_TYPE, device_type);
    
    // Set the root directory pointer
    DirectoryHandle handle = fsDirectoryCreate(*part, device_name);
    
    uint8_t ptrBytes[4];
    *((uint32_t*)&ptrBytes[0]) = handle;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part->block_address + i + DEVICE_OFFSET_ROOT, ptrBytes[i]);
    
    // Set the sector size
    *((uint32_t*)&sizeBytes[0]) = part->sector_size;
    for (uint8_t i=0; i < 4; i++) 
        fs_write_byte(part->block_address + i + DEVICE_OFFSET_SECT_SZ, sizeBytes[i]);
    
    return 1;
}

