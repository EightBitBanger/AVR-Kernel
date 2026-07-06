//#include <kernel/arch/x86/drivers/rng.h>
#include <stdint.h>
#include <limits.h>

#include <kernel/vfs/vfs.h>
#include <kernel/util/string.h>

#define RAND_MAX_INT      0x7FFFFFFF

static uint32_t fallback_seed = 123456789;

struct RandFileHeader {
    char id_header[8];
    
    uint32_t current_seed;
};

void rand_seed(unsigned int seed) {
    if (seed == 0) 
        seed++;
    fallback_seed = seed;
}

int rand(void) {
    fallback_seed = fallback_seed * 1103515245 + 12345;
    return (int)((fallback_seed / 65536) % (RAND_MAX_INT));
}

void rand_init(void) {
    const char* rand_file_path = "/mnt/ata0/sys/urand";
    vfs_set_permissions(rand_file_path, VFS_PERMISSION_READ | VFS_PERMISSION_WRITE);
    
    File file = vfs_open(rand_file_path, VFS_OPEN_READ | VFS_OPEN_WRITE);
    if (file == VFS_INVALID_FILE) {
        file = vfs_open(rand_file_path, VFS_OPEN_CREATE | VFS_OPEN_READ | VFS_OPEN_WRITE);
        if (file == VFS_INVALID_FILE) 
            return;
        
        struct RandFileHeader header;
        memcpy(header.id_header, "RND!", 4);
        header.current_seed = 42;
        vfs_write(file, &header, sizeof(struct RandFileHeader));
    }
    
    struct RandFileHeader header;
    vfs_seek(file, 0);
    vfs_read(file, &header, sizeof(struct RandFileHeader));
    
    // Check header
    if (memcmp(header.id_header, "RND!", 4) != 0) {
        vfs_close(file);
        return;
    }
    
    rand_seed(header.current_seed);
    header.current_seed += rand();
    
    vfs_seek(file, 0);
    vfs_write(file, &header, sizeof(struct RandFileHeader));
    vfs_close(file);
    
    vfs_set_permissions(rand_file_path, VFS_PERMISSION_READ);
}
