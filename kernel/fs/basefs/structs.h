#ifndef BASE_FILE_STRUCTS_H
#define BASE_FILE_STRUCTS_H

#include <kernel/fs/config.h>

#include <stdint.h>
#include <stdbool.h>

#define FS_DIRECTORY_REF_MAX         6
#define FS_DIRECTORY_EX_REF_MAX     12

struct __attribute__((packed)) FSDeviceHeader {
    uint8_t id;
    char name[10];
    uint8_t pad[5];
};

struct __attribute__((packed)) FSPartitionBlock {
    uint32_t total_size;
    uint32_t sector_size;
    
    uint32_t root_directory;
    
    char name[FS_NAME_LENGTH_MAX];
    
    uint8_t  flags;
    uint8_t  type;
    
    uint8_t  reserved;
    uint8_t  magic;
    // Total 32 bytes
};

struct __attribute__((packed)) FSAllocHeader {
    uint32_t size; // 4 bytes
};

struct __attribute__((packed)) FSBlockHeader {
    char    name[FS_NAME_LENGTH_MAX]; // 16 bytes
    
    uint8_t attributes;               // 1 byte
    uint8_t permissions;              // 1 byte
    
    uint8_t padding[6];               // 6 bytes of padding to align fields to 4-byte/32-byte boundary
    
    uint32_t security;                // 4 bytes
    uint32_t certificate;             // 4 bytes
    // Total 32 bytes
};

struct __attribute__((packed)) FSFileHeader {
    struct FSBlockHeader block;       // 32 bytes
};

struct __attribute__((packed)) FSDirectoryReference {
    uint32_t ref;                     // 4 bytes
};

struct __attribute__((packed)) FSDirectoryHeader {
    struct FSBlockHeader block;       // 32 bytes
    
    uint32_t parent;                  // 4 bytes
    
    uint32_t next;                    // 4 bytes
    uint32_t prev;                    // 4 bytes
    
    uint32_t reference_count;         // 4 bytes
    struct FSDirectoryReference reference[FS_DIRECTORY_REF_MAX]; // 6 * 4 = 24 bytes
    // Total 72 bytes
};

struct __attribute__((packed)) FSDirectoryExtent {
    uint32_t next;                    // 4 bytes
    uint32_t prev;                    // 4 bytes
    
    uint32_t reference_count;         // 4 bytes
    struct FSDirectoryReference reference[FS_DIRECTORY_EX_REF_MAX]; // 12 * 4 = 48 bytes
    // Total 60 bytes
};

#endif
