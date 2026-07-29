#include <kernel/fs/fs.h>
#include <kernel/fs/basefs/file.h>
#include <kernel/fs/config.h>

#include <kernel/util/string.h>

bool fs_file_check(struct FSDeviceContext* ctx, uint32_t address) {
    if (!ctx) return false;
    struct FSAllocHeader alloc;
    struct FSFileHeader  file;
    if (address == FS_NULL)
        return false;
    fs_mem_read(ctx, address - sizeof(struct FSAllocHeader), &alloc, sizeof(struct FSAllocHeader));
    if (alloc.size < sizeof(struct FSFileHeader))
        return false;
    fs_mem_read(ctx, address, &file, sizeof(struct FSFileHeader));
    
    if (file.block.attributes & FS_ATTRIBUTE_DIRECTORY) 
        return false;
    return true;
}

static bool fs_file_struct_is_valid(const FileHandle* file) {
    if (file == NULL || file->ctx == NULL)
        return false;
    if (!file->is_open)
        return false;
    if (!fs_file_check(file->ctx, file->address))
        return false;
    return true;
}

static bool fs_file_extent_read(struct FSDeviceContext* ctx, uint32_t extent_address, struct FSFileExtent* extent) {
    struct FSAllocHeader alloc_header;
    if (extent_address == FS_NULL) return false;
    fs_mem_read(ctx, extent_address - sizeof(struct FSAllocHeader), &alloc_header, sizeof(struct FSAllocHeader));
    if (alloc_header.size < sizeof(struct FSFileExtent))
        return false;
    fs_mem_read(ctx, extent_address, extent, sizeof(struct FSFileExtent));
    return true;
}

static void fs_file_extent_write(struct FSDeviceContext* ctx, uint32_t extent_address, const struct FSFileExtent* extent) {
    fs_mem_write(ctx, extent_address, extent, sizeof(struct FSFileExtent));
}

uint32_t fs_file_create(struct FSDeviceContext* ctx, const char* name, uint8_t permissions, uint32_t size, uint32_t parent_directory) {
    struct FSFileHeader file;
    uint32_t            address;
    
    address = fs_alloc(ctx, sizeof(struct FSFileHeader));
    if (address == FS_NULL) 
        return FS_NULL;
    if (parent_directory != FS_NULL) 
        fs_directory_add_reference(ctx, parent_directory, address);
    
    memset(&file, 0x00, sizeof(struct FSFileHeader));
    strncpy(file.block.name, name, sizeof(file.block.name) - 1);
    file.block.name[sizeof(file.block.name) - 1] = '\0';
    file.block.attributes  = 0;
    file.block.permissions = permissions;
    file.total_logical_size = 0;
    file.extent.next = FS_NULL;
    file.extent.prev = FS_NULL;
    
    fs_mem_write(ctx, address, &file, sizeof(struct FSFileHeader));
    if (size > 0) {
        if (!fs_file_resize(ctx, address, size)) {
            fs_free(ctx, address);
            return FS_NULL;
        }
    }
    
    return address;
}

bool fs_file_delete(struct FSDeviceContext* ctx, uint32_t address) {
    struct FSFileHeader file;
    uint32_t extent_address;
    uint32_t next_extent;
    if (!fs_file_check(ctx, address))
        return false;
    
    fs_mem_read(ctx, address, &file, sizeof(struct FSFileHeader));
    extent_address = file.extent.next;
    while (extent_address != FS_NULL) {
        struct FSFileExtent extent;
        if (!fs_file_extent_read(ctx, extent_address, &extent))
            break;
            
        next_extent = extent.extent.next;
        fs_free(ctx, extent_address);
        extent_address = next_extent;
    }
    
    fs_free(ctx, address);
    return true;
}

uint32_t fs_file_get_size(struct FSDeviceContext* ctx, uint32_t address) {
    struct FSFileHeader file;
    if (!fs_file_check(ctx, address))
        return 0;
    fs_mem_read(ctx, address, &file, sizeof(struct FSFileHeader));
    return file.total_logical_size;
}

bool fs_file_get_name(struct FSDeviceContext* ctx, uint32_t address, char* filename) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    strncpy(filename, header.block.name, FS_NAME_LENGTH_MAX);
    return true;
}

bool fs_file_set_name(struct FSDeviceContext* ctx, uint32_t address, const char* filename) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    uint8_t permissions = header.block.permissions;
    if (!(permissions & FS_PERMISSION_WRITE)) 
        return false;
    size_t length = strlen(filename) + 1;
    if (length > FS_NAME_LENGTH_MAX) 
        length = FS_NAME_LENGTH_MAX;
    if (length == 0) 
        return false;
    memcpy(header.block.name, filename, length);
    fs_mem_write(ctx, address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_permissions(struct FSDeviceContext* ctx, uint32_t address, uint8_t* permissions) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    *permissions = header.block.permissions;
    return true;
}

bool fs_file_set_permissions(struct FSDeviceContext* ctx, uint32_t address, uint8_t permissions) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    header.block.permissions = permissions;
    fs_mem_write(ctx, address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_attributes(struct FSDeviceContext* ctx, uint32_t address, uint8_t* attributes) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    *attributes = header.block.attributes;
    return true;
}

bool fs_file_set_attributes(struct FSDeviceContext* ctx, uint32_t address, uint8_t attributes) {
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    header.block.attributes = attributes;
    fs_mem_write(ctx, address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_open(FileHandle* file, struct FSDeviceContext* ctx, uint32_t address, uint8_t mode) {
    struct FSFileHeader header;
    uint32_t            size;
    
    memset(file, 0x00, sizeof(FileHandle));
    if (!fs_file_check(ctx, address)) 
        return false;
    
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    if ((mode & FS_FILE_MODE_READ) && !(header.block.permissions & FS_PERMISSION_READ)) 
        return false;
    if ((mode & FS_FILE_MODE_WRITE) && !(header.block.permissions & FS_PERMISSION_WRITE)) 
        return false;
    file->ctx     = ctx;
    file->address = address;
    file->mode    = mode;
    file->is_open = 1;
    if (mode & FS_FILE_MODE_APPEND) {
        size = fs_file_get_size(ctx, address);
        file->position = size;
    } else {
        file->position = 0;
    }
    
    return true;
}

void fs_file_close(FileHandle* file) {
    if (file == NULL)
        return;
    file->ctx       = NULL;
    file->address   = FS_NULL;
    file->position  = 0;
    file->mode      = 0;
    file->is_open   = 0;
}

uint32_t fs_file_read(FileHandle* file, void* destination, uint32_t size) {
    struct FSFileHeader f_header;
    uint32_t file_size;
    uint32_t bytes_to_read;
    
    if (!fs_file_struct_is_valid(file)) return 0;
    if (!(file->mode & FS_FILE_MODE_READ)) return 0;
    if (size == 0) return 0;
    file_size = fs_file_get_size(file->ctx, file->address);
    if (file->position >= file_size) return 0;
    
    bytes_to_read = size;
    if ((file->position + bytes_to_read) > file_size)
        bytes_to_read = file_size - file->position;
    fs_mem_read(file->ctx, file->address, &f_header, sizeof(struct FSFileHeader));
    
    uint32_t current_extent_addr = f_header.extent.next;
    uint32_t read_offset = file->position;
    uint32_t destination_cursor = (uint32_t)destination;
    uint32_t remaining_bytes = bytes_to_read;
    uint32_t accumulated_boundary = 0;
    
    while (current_extent_addr != FS_NULL && remaining_bytes > 0) {
        struct FSFileExtent ext;
        if (!fs_file_extent_read(file->ctx, current_extent_addr, &ext)) break;
        
        if (read_offset >= accumulated_boundary && read_offset < accumulated_boundary + ext.extent_data_size) {
            uint32_t internal_block_offset = read_offset - accumulated_boundary;
            uint32_t bytes_available_in_block = ext.extent_data_size - internal_block_offset;
            uint32_t chunk = (remaining_bytes > bytes_available_in_block) ? bytes_available_in_block : remaining_bytes;
            fs_mem_read(file->ctx, current_extent_addr + sizeof(struct FSFileExtent) + internal_block_offset, 
                        (void*)destination_cursor, chunk);
            destination_cursor += chunk;
            read_offset += chunk;
            remaining_bytes -= chunk;
        }
        accumulated_boundary += ext.extent_data_size;
        current_extent_addr = ext.extent.next;
    }
    
    file->position += (bytes_to_read - remaining_bytes);
    return (bytes_to_read - remaining_bytes);
}

uint32_t fs_file_write(FileHandle* file, const void* source, uint32_t size) {
    struct FSFileHeader f_header;
    uint32_t file_size;
    uint32_t bytes_to_write;
    if (!fs_file_struct_is_valid(file)) return 0;
    if (!(file->mode & FS_FILE_MODE_WRITE)) return 0;
    if (size == 0) return 0;
    
    file_size = fs_file_get_size(file->ctx, file->address);
    if ((file->position + size) > file_size) {
        if (!fs_file_resize(file->ctx, file->address, file->position + size))
            return 0;
    }
    
    bytes_to_write = size;
    fs_mem_read(file->ctx, file->address, &f_header, sizeof(struct FSFileHeader));
    
    uint32_t current_extent_addr = f_header.extent.next;
    uint32_t write_offset = file->position;
    uint32_t source_cursor = (uint32_t)source;
    uint32_t remaining_bytes = bytes_to_write;
    uint32_t accumulated_boundary = 0;
    while (current_extent_addr != FS_NULL && remaining_bytes > 0) {
        struct FSFileExtent ext;
        if (!fs_file_extent_read(file->ctx, current_extent_addr, &ext)) break;
        
        if (write_offset >= accumulated_boundary && write_offset < accumulated_boundary + ext.extent_data_size) {
            uint32_t internal_block_offset = write_offset - accumulated_boundary;
            uint32_t bytes_available_in_block = ext.extent_data_size - internal_block_offset;
            uint32_t chunk = (remaining_bytes > bytes_available_in_block) ? bytes_available_in_block : remaining_bytes;
            fs_mem_write(file->ctx, current_extent_addr + sizeof(struct FSFileExtent) + internal_block_offset, 
                         (const void*)source_cursor, chunk);
            source_cursor += chunk;
            write_offset += chunk;
            remaining_bytes -= chunk;
        }
        accumulated_boundary += ext.extent_data_size;
        current_extent_addr = ext.extent.next;
    }
    
    file->position += (bytes_to_write - remaining_bytes);
    return (bytes_to_write - remaining_bytes);
}

bool fs_file_resize(struct FSDeviceContext* ctx, uint32_t address, uint32_t new_size) {
    if (!ctx) return false;
    struct FSFileHeader file;
    if (!fs_file_check(ctx, address))
        return false;
        
    fs_mem_read(ctx, address, &file, sizeof(struct FSFileHeader));
    if (file.total_logical_size == new_size)
        return true;

    if (new_size < file.total_logical_size) {
        uint32_t accumulated = 0;
        uint32_t current_extent_addr = file.extent.next;
        uint32_t prev_extent_addr = address;
        bool is_first = true;
        while (current_extent_addr != FS_NULL) {
            struct FSFileExtent ext;
            if (!fs_file_extent_read(ctx, current_extent_addr, &ext)) break;
            
            if (accumulated + ext.extent_data_size > new_size) {
                uint32_t keep_bytes = new_size - accumulated;
                if (keep_bytes == 0) {
                    if (is_first) {
                        file.extent.next = FS_NULL;
                    } else {
                        struct FSFileExtent p_ext;
                        fs_file_extent_read(ctx, prev_extent_addr, &p_ext);
                        p_ext.extent.next = FS_NULL;
                        fs_file_extent_write(ctx, prev_extent_addr, &p_ext);
                    }
                    
                    uint32_t free_addr = current_extent_addr;
                    while (free_addr != FS_NULL) {
                        struct FSFileExtent f_ext;
                        fs_file_extent_read(ctx, free_addr, &f_ext);
                        uint32_t next_free = f_ext.extent.next;
                        fs_free(ctx, free_addr);
                        free_addr = next_free;
                    }
                    break;
                } else {
                    ext.extent_data_size = keep_bytes;
                    uint32_t free_addr = ext.extent.next;
                    ext.extent.next = FS_NULL;
                    fs_file_extent_write(ctx, current_extent_addr, &ext);
                    
                    while (free_addr != FS_NULL) {
                        struct FSFileExtent f_ext;
                        fs_file_extent_read(ctx, free_addr, &f_ext);
                        uint32_t next_free = f_ext.extent.next;
                        fs_free(ctx, free_addr);
                        free_addr = next_free;
                    }
                    break;
                }
            }
            accumulated += ext.extent_data_size;
            prev_extent_addr = current_extent_addr;
            current_extent_addr = ext.extent.next;
            is_first = false;
        }
        file.total_logical_size = new_size;
        fs_mem_write(ctx, address, &file, sizeof(struct FSFileHeader));
        return true;
    }
    
    uint32_t growth_needed = new_size - file.total_logical_size;
    uint32_t current_extent_addr = file.extent.next;
    uint32_t tail_address = address;
    bool chain_is_empty = true; 
    struct FSFileExtent tail_extent;
    while (current_extent_addr != FS_NULL) {
        if (!fs_file_extent_read(ctx, current_extent_addr, &tail_extent)) return false;
        tail_address = current_extent_addr;
        current_extent_addr = tail_extent.extent.next;
        chain_is_empty = false;
    }
    
    uint32_t default_extent_payload_limit = 4096; 
    while (growth_needed > 0) {
        uint32_t chunk_allocation = growth_needed;
        if (chunk_allocation > default_extent_payload_limit) {
            chunk_allocation = default_extent_payload_limit;
        }
        
        uint32_t overhead = sizeof(struct FSAllocHeader) + sizeof(struct FSFileExtent); // 16 bytes
        if (chunk_allocation >= overhead && (chunk_allocation % ctx->sector_size == 0)) {
            chunk_allocation -= overhead;
        }
        
        uint32_t new_ext_addr = fs_alloc(ctx, sizeof(struct FSFileExtent) + chunk_allocation);
        if (new_ext_addr == FS_NULL) return false;
        
        struct FSFileExtent new_ext;
        memset(&new_ext, 0x00, sizeof(struct FSFileExtent));
        new_ext.extent.prev = tail_address;
        new_ext.extent.next = FS_NULL;
        new_ext.extent_data_size = chunk_allocation;
        fs_file_extent_write(ctx, new_ext_addr, &new_ext);
        
        uint8_t zero[16] = {0};
        uint32_t zero_remains = chunk_allocation;
        uint32_t write_ptr = new_ext_addr + sizeof(struct FSFileExtent);
        while (zero_remains > 0) {
            uint32_t z_chunk = (sizeof(zero) > zero_remains) ? zero_remains : sizeof(zero);
            fs_mem_write(ctx, write_ptr, zero, z_chunk);
            write_ptr += z_chunk;
            zero_remains -= z_chunk;
        }
        
        if (chain_is_empty) {
            file.extent.next = new_ext_addr;
            chain_is_empty = false; 
        } else {
            tail_extent.extent.next = new_ext_addr;
            fs_file_extent_write(ctx, tail_address, &tail_extent);
        }
        
        tail_address = new_ext_addr;
        tail_extent = new_ext;
        growth_needed -= chunk_allocation;
    }
    
    file.total_logical_size = new_size;
    fs_mem_write(ctx, address, &file, sizeof(struct FSFileHeader));
    return true;
}

uint32_t fs_file_seek(FileHandle* file, uint32_t position) {
    uint32_t file_size;
    if (!fs_file_struct_is_valid(file))
        return FS_NULL;
    
    file_size = fs_file_get_size(file->ctx, file->address);
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
