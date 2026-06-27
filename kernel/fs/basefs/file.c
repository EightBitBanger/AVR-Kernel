#include <kernel/fs/fs.h>
#include <kernel/fs/basefs/file.h>
#include <kernel/fs/config.h>

#include <kernel/util/string.h>

bool fs_file_check(uint32_t address) {
    struct FSAllocHeader alloc;
    struct FSFileHeader  file;
    
    if (address == FS_NULL)
        return false;
    
    fs_mem_read(address - sizeof(struct FSAllocHeader), &alloc, sizeof(struct FSAllocHeader));
    if (alloc.size < sizeof(struct FSFileHeader))
        return false;
    
    fs_mem_read(address, &file, sizeof(struct FSFileHeader));
    
    if (file.block.attributes & FS_ATTRIBUTE_DIRECTORY) 
        return false;
    
    return true;
}

static bool fs_file_struct_is_valid(const FileHandle* file) {
    if (file == NULL)
        return false;
    
    if (!file->is_open)
        return false;
    
    if (!fs_file_check(file->address))
        return false;
    
    return true;
}

uint32_t fs_file_create(const char* name, uint8_t permissions, uint32_t size, uint32_t parent_directory) {
    struct FSFileHeader file;
    uint32_t            address;
    uint32_t            payload_address;
    uint32_t            remaining;
    uint8_t             zero_buffer[16];
    
    address = fs_alloc(sizeof(struct FSFileHeader) + size);
    if (address == FS_NULL) 
        return FS_NULL;
    
    if (parent_directory != FS_NULL) 
        fs_directory_add_reference(parent_directory, address);
    
    memset(&file, 0x00, sizeof(struct FSFileHeader));
    strncpy(file.block.name, name, sizeof(file.block.name) - 1);
    file.block.name[sizeof(file.block.name) - 1] = '\0';
    file.block.attributes  = 0;
    file.block.permissions = permissions;
    
    fs_mem_write(address, &file, sizeof(struct FSFileHeader));
    
    memset(zero_buffer, 0x00, sizeof(zero_buffer));
    
    payload_address = address + sizeof(struct FSFileHeader);
    remaining       = size;
    
    while (remaining > 0) {
        uint32_t chunk_size = sizeof(zero_buffer);
        
        if (chunk_size > remaining)
            chunk_size = remaining;
        
        fs_mem_write(payload_address, zero_buffer, chunk_size);
        
        payload_address += chunk_size;
        remaining       -= chunk_size;
    }
    
    return address;
}

bool fs_file_delete(uint32_t address) {
    if (!fs_file_check(address))
        return false;
    
    fs_free(address);
    return true;
}

bool fs_file_resize(uint32_t address, uint32_t new_size) {
    
    //
    // TODO FIX FIRST reallocation will BREAK the parent directories reference address
    //
    
    /*
    if (!fs_file_check(address))
        return false;
    
    uint32_t alloc_addr = address - sizeof(struct FSAllocHeader);
    struct FSAllocHeader old_header;
    fs_mem_read(alloc_addr, &old_header, sizeof(struct FSAllocHeader));
    
    uint32_t current_size = old_header.size;
    if (current_size == new_size)
        return true; // Already the correct size
    
    // External global filesystem variables declared in device.c
    extern uint32_t fs_sector_size;
    extern uint32_t fs_block_count;
    
    // Calculate block requirements
    uint32_t current_blocks = (sizeof(struct FSAllocHeader) + current_size + fs_sector_size - 1UL) / fs_sector_size;
    uint32_t new_blocks     = (sizeof(struct FSAllocHeader) + new_size + fs_sector_size - 1UL) / fs_sector_size;
    uint32_t start_block    = alloc_addr / fs_sector_size;
    
    // Shrinking the file
    if (new_size < current_size) {
        // If the number of blocks needed drops, free the trailing blocks
        if (new_blocks < current_blocks) {
            for (uint32_t i = new_blocks; i < current_blocks; i++) {
                fs_bitmap_clear(start_block + i);
            }
            fs_bitmap_flush();
        }
        
        // Update header size metadata
        old_header.size = new_size;
        fs_mem_write(alloc_addr, &old_header, sizeof(struct FSAllocHeader));
        return true;
    }
    
    // Grow the file
    uint32_t extra_blocks_needed = new_blocks - current_blocks;
    
    // Check if the blocks directly following this file run are free
    bool contiguous_space_available = true;
    uint32_t next_block_start = start_block + current_blocks;
    
    if (next_block_start + extra_blocks_needed <= fs_block_count) {
        for (uint32_t i = 0; i < extra_blocks_needed; i++) {
            if (fs_bitmap_get(next_block_start + i)) {
                contiguous_space_available = false;
                break;
            }
        }
    } else {
        contiguous_space_available = false;
    }
    
    // Check to expand in-place
    if (contiguous_space_available) {
        for (uint32_t i = 0; i < extra_blocks_needed; i++) {
            fs_bitmap_set(next_block_start + i);
        }
        fs_bitmap_flush();
    
        // Zero-out the newly appended payload region
        uint32_t zero_start_addr = address + current_size;
        uint32_t zero_bytes_remaining = new_size - current_size;
        uint8_t zero_buffer[16] = {0};
    
        while (zero_bytes_remaining > 0) {
            uint32_t chunk = sizeof(zero_buffer);
            if (chunk > zero_bytes_remaining) chunk = zero_bytes_remaining;
            fs_mem_write(zero_start_addr, zero_buffer, chunk);
            zero_start_addr += chunk;
            zero_bytes_remaining -= chunk;
        }
    
        // Update header with the expanded size
        old_header.size = new_size;
        fs_mem_write(alloc_addr, &old_header, sizeof(struct FSAllocHeader));
        return true;
    }
    
    // Out of space locally. We must relocate the file.
    // Allocate an entirely new space to hold everything
    uint32_t new_address = fs_alloc(new_size);
    if (new_address == FS_NULL)
        return false; // Disk full or fragmented!
    
    // Copy file header and payload over to the new block run
    // Total size to copy is the smaller of header + current_size
    uint32_t data_to_copy = sizeof(struct FSFileHeader) + current_size;
    
    // Simple block copy loop
    uint8_t copy_buffer[32];
    uint32_t copy_src = address;
    uint32_t copy_dst = new_address;
    uint32_t copy_remaining = data_to_copy;
    
    while (copy_remaining > 0) {
        uint32_t chunk = sizeof(copy_buffer);
        if (chunk > copy_remaining) chunk = copy_remaining;
        
        fs_mem_read(copy_src, copy_buffer, chunk);
        fs_mem_write(copy_dst, copy_buffer, chunk);
        
        copy_src += chunk;
        copy_dst += chunk;
        copy_remaining -= chunk;
    }
    
    // Free the old contiguous block space
    fs_free(address);
    
    return new_address;
    */
}

uint32_t fs_file_get_size(uint32_t address) {
    struct FSAllocHeader header;
    
    if (!fs_file_check(address))
        return 0;
    
    fs_mem_read(address - sizeof(struct FSAllocHeader), &header, sizeof(struct FSAllocHeader));
    
    if (header.size < sizeof(struct FSFileHeader))
        return 0;
    
    return header.size - sizeof(struct FSFileHeader);
}

bool fs_file_get_name(uint32_t address, char* filename) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    strncpy(filename, header.block.name, FS_NAME_LENGTH_MAX);
    return true;
}

bool fs_file_set_name(uint32_t address, const char* filename) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    uint8_t permissions = header.block.permissions;
    if (!(permissions & FS_PERMISSION_WRITE)) 
        return false;
    size_t length = strlen(filename) + 1;
    if (length > FS_NAME_LENGTH_MAX) 
        length = FS_NAME_LENGTH_MAX;
    if (length == 0) 
        return false;
    memcpy(header.block.name, filename, length);
    fs_mem_write(address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_permissions(uint32_t address, uint8_t* permissions) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    *permissions = header.block.permissions;
    return true;
}

bool fs_file_set_permissions(uint32_t address, uint8_t permissions) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    header.block.permissions = permissions;
    fs_mem_write(address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_attributes(uint32_t address, uint8_t* attributes) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    *attributes = header.block.attributes;
    return true;
}

bool fs_file_set_attributes(uint32_t address, uint8_t attributes) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    header.block.attributes = attributes;
    fs_mem_write(address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_open(FileHandle* file, uint32_t address, uint8_t mode) {
    struct FSFileHeader header;
    uint32_t            size;
    
    memset(file, 0x00, sizeof(FileHandle));
    
    if (!fs_file_check(address)) 
        return false;
    
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    
    if ((mode & FS_FILE_MODE_READ) && !(header.block.permissions & FS_PERMISSION_READ)) 
        return false;
    if ((mode & FS_FILE_MODE_WRITE) && !(header.block.permissions & FS_PERMISSION_WRITE)) 
        return false;
    
    file->address = address;
    file->mode    = mode;
    file->is_open = 1;
    
    if (mode & FS_FILE_MODE_APPEND) {
        size = fs_file_get_size(address);
        file->position = size;
    } else {
        file->position = 0;
    }
    
    return true;
}

void fs_file_close(FileHandle* file) {
    if (file == NULL)
        return;
    
    file->address   = FS_NULL;
    file->position  = 0;
    file->mode      = 0;
    file->is_open   = 0;
}

uint32_t fs_file_read(FileHandle* file, void* destination, uint32_t size) {
    uint32_t file_size;
    uint32_t bytes_to_read;
    
    if (!fs_file_struct_is_valid(file))
        return 0;
    if (!(file->mode & FS_FILE_MODE_READ))
        return 0;
    if (size == 0)
        return 0;
    
    file_size = fs_file_get_size(file->address);
    
    if (file->position >= file_size)
        return 0;
    
    bytes_to_read = size;
    if ((file->position + bytes_to_read) > file_size)
        bytes_to_read = file_size - file->position;
    
    fs_mem_read(file->address + sizeof(struct FSFileHeader) + file->position,
                destination,
                bytes_to_read);
    
    file->position += bytes_to_read;
    
    return bytes_to_read;
}

uint32_t fs_file_write(FileHandle* file, const void* source, uint32_t size) {
    uint32_t file_size;
    uint32_t bytes_to_write;
    
    if (!fs_file_struct_is_valid(file))
        return 0;
    if (!(file->mode & FS_FILE_MODE_WRITE))
        return 0;
    if (size == 0)
        return 0;
    
    file_size = fs_file_get_size(file->address);
    
    if (file->position >= file_size)
        return 0;
    
    bytes_to_write = size;
    if ((file->position + bytes_to_write) > file_size)
        bytes_to_write = file_size - file->position;
    
    fs_mem_write(file->address + sizeof(struct FSFileHeader) + file->position,
                 source,
                 bytes_to_write);
    
    file->position += bytes_to_write;
    
    return bytes_to_write;
}

uint32_t fs_file_seek(FileHandle* file, uint32_t position) {
    uint32_t file_size;
    if (!fs_file_struct_is_valid(file))
        return FS_NULL;
    
    file_size = fs_file_get_size(file->address);
    if (position > file_size)
        position = file_size;
    
    file->position = position;
    return position;
}

uint32_t fs_file_tell(const FileHandle* file) {
    if (file == NULL) 
        return FS_NULL;
    if (!file->is_open) 
        return FS_NULL;
    return file->position;
}

uint32_t fs_file_get_address(const FileHandle* file) {
    if (file == NULL) 
        return FS_NULL;
    if (!file->is_open) 
        return FS_NULL;
    return file->address;
}
