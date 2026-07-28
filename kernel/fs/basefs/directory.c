#include <kernel/fs/fs.h>
#include <kernel/fs/basefs/directory.h>

#include <kernel/util/string.h>

static bool fs_file_alloc_header_read(struct FSDeviceContext* ctx, uint32_t payload_address, struct FSAllocHeader* header) {
    uint32_t allocation_address;
    if (payload_address == FS_NULL)
        return false;
    if (payload_address < sizeof(struct FSAllocHeader))
        return false;
    
    allocation_address = payload_address - sizeof(struct FSAllocHeader);
    fs_mem_read(ctx, allocation_address, header, sizeof(struct FSAllocHeader));
    
    if (header->size == 0)
        return false;
    
    return true;
}

static uint32_t fs_directory_header_max_refs(struct FSDeviceContext* ctx, uint32_t directory_address) {
    struct FSAllocHeader alloc_header;
    if (!fs_file_alloc_header_read(ctx, directory_address, &alloc_header))
        return 0;
    if (alloc_header.size <= sizeof(struct FSDirectoryHeader))
        return 0;
    return (alloc_header.size - sizeof(struct FSDirectoryHeader)) / sizeof(uint32_t);
}

static uint32_t fs_directory_extent_max_refs(struct FSDeviceContext* ctx, uint32_t extent_address) {
    struct FSAllocHeader alloc_header;
    if (!fs_file_alloc_header_read(ctx, extent_address, &alloc_header))
        return 0;
    if (alloc_header.size <= sizeof(struct FSDirectoryExtent))
        return 0;
    return (alloc_header.size - sizeof(struct FSDirectoryExtent)) / sizeof(uint32_t);
}

bool fs_directory_header_read(struct FSDeviceContext* ctx, uint32_t directory_address, struct FSDirectoryHeader* directory) {
    struct FSAllocHeader alloc_header;
    if (!fs_file_alloc_header_read(ctx, directory_address, &alloc_header))
        return false;
    if (alloc_header.size < sizeof(struct FSDirectoryHeader))
        return false;
    
    fs_mem_read(ctx, directory_address, directory, sizeof(struct FSDirectoryHeader));
    return true;
}

void fs_directory_header_write(struct FSDeviceContext* ctx, uint32_t directory_address, const struct FSDirectoryHeader* directory) {
    fs_mem_write(ctx, directory_address, directory, sizeof(struct FSDirectoryHeader));
}

bool fs_directory_extent_read(struct FSDeviceContext* ctx, uint32_t extent_address, struct FSDirectoryExtent* extent) {
    struct FSAllocHeader alloc_header;
    if (!fs_file_alloc_header_read(ctx, extent_address, &alloc_header))
        return false;
    if (alloc_header.size < sizeof(struct FSDirectoryExtent))
        return false;
    
    fs_mem_read(ctx, extent_address, extent, sizeof(struct FSDirectoryExtent));
    return true;
}

void fs_directory_extent_write(struct FSDeviceContext* ctx, uint32_t extent_address, const struct FSDirectoryExtent* extent) {
    fs_mem_write(ctx, extent_address, extent, sizeof(struct FSDirectoryExtent));
}

uint32_t fs_directory_extent_create(struct FSDeviceContext* ctx, uint32_t prev_address, uint32_t initial_capacity) {
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;
    extent_address = fs_alloc(ctx, sizeof(struct FSDirectoryExtent) + (initial_capacity * sizeof(uint32_t)));
    if (extent_address == FS_NULL)
        return FS_NULL;
    memset(&extent, 0x00, sizeof(struct FSDirectoryExtent));
    extent.extent.prev = prev_address;
    extent.extent.next = FS_NULL;
    extent.reference_count = 0;
    
    fs_directory_extent_write(ctx, extent_address, &extent);
    
    return extent_address;
}

void fs_directory_extent_unlink_and_free(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t extent_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    struct FSDirectoryExtent prev_extent;
    struct FSDirectoryExtent next_extent;
    
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return;
    if (!fs_directory_extent_read(ctx, extent_address, &extent))
        return;
    if (extent.extent.prev == directory_address) {
        directory.extent.next = extent.extent.next;
        fs_directory_header_write(ctx, directory_address, &directory);
    } else if (extent.extent.prev != FS_NULL) {
        if (fs_directory_extent_read(ctx, extent.extent.prev, &prev_extent)) {
            prev_extent.extent.next = extent.extent.next;
            fs_directory_extent_write(ctx, extent.extent.prev, &prev_extent);
        }
    }
    
    if (extent.extent.next != FS_NULL) {
        if (fs_directory_extent_read(ctx, extent.extent.next, &next_extent)) {
            next_extent.extent.prev = extent.extent.prev;
            fs_directory_extent_write(ctx, extent.extent.next, &next_extent);
        }
    }
    
    fs_free(ctx, extent_address);
}

uint32_t fs_directory_create(struct FSDeviceContext* ctx, const char* name, uint8_t permissions, uint32_t parent_directory) {
    uint32_t initial_capacity = 24;
    uint32_t address = fs_alloc(ctx, sizeof(struct FSDirectoryHeader) + (initial_capacity * sizeof(uint32_t)));
    if (address == FS_NULL)
        return address;
    if (parent_directory != FS_NULL) 
        fs_directory_add_reference(ctx, parent_directory, address);
    
    struct FSDirectoryHeader directory;
    memset(&directory, 0x00, sizeof(struct FSDirectoryHeader));
    strncpy(directory.block.name, name, sizeof(directory.block.name) - 1);
    directory.block.attributes  = FS_ATTRIBUTE_DIRECTORY;
    directory.block.permissions = permissions;
    directory.parent            = parent_directory;
    directory.extent.next       = FS_NULL;
    directory.extent.prev       = FS_NULL;
    directory.reference_count   = 0;
    
    fs_directory_header_write(ctx, address, &directory);
    return address;
}

bool fs_directory_delete(struct FSDeviceContext* ctx, uint32_t address) {
    struct FSDirectoryHeader directory;
    uint32_t                 extent_address;
    uint32_t                 next_extent_address;
    if (!fs_directory_header_read(ctx, address, &directory))
        return false;
    
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        struct FSDirectoryExtent extent;
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            break;
        
        next_extent_address = extent.extent.next;
        fs_free(ctx, extent_address);
        extent_address = next_extent_address;
    }
    
    fs_free(ctx, address);
    return true;
}

uint8_t fs_directory_add_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    struct FSDirectoryExtent tail_extent;
    uint32_t                 extent_address;
    uint32_t                 new_extent_address;
    uint32_t                 index;
    uint32_t                 current_ref;
    if (reference_address == FS_NULL)
        return 1;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return 2;
    
    uint32_t dir_max_refs = fs_directory_header_max_refs(ctx, directory_address);
    for (index = 0; index < directory.reference_count; index++) {
        uint32_t ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (index * sizeof(uint32_t));
        fs_mem_read(ctx, ref_offset, &current_ref, sizeof(uint32_t));
        if (current_ref == reference_address)
            return 0;
    }
    
    if (directory.reference_count < dir_max_refs) {
        uint32_t ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (directory.reference_count * sizeof(uint32_t));
        fs_mem_write(ctx, ref_offset, &reference_address, sizeof(uint32_t));
        directory.reference_count++;
        fs_directory_header_write(ctx, directory_address, &directory);
        return 0;
    }
    
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            return 3;
        uint32_t ext_max_refs = fs_directory_extent_max_refs(ctx, extent_address);
        
        for (index = 0; index < extent.reference_count; index++) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (index * sizeof(uint32_t));
            fs_mem_read(ctx, ref_offset, &current_ref, sizeof(uint32_t));
            if (current_ref == reference_address)
                return 0;
        }
        
        if (extent.reference_count < ext_max_refs) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (extent.reference_count * sizeof(uint32_t));
            fs_mem_write(ctx, ref_offset, &reference_address, sizeof(uint32_t));
            extent.reference_count++;
            fs_directory_extent_write(ctx, extent_address, &extent);
            return 0;
        }
        
        if (extent.extent.next == FS_NULL)
            break;
        extent_address = extent.extent.next;
    }
    
    if (directory.extent.next == FS_NULL) {
        new_extent_address = fs_directory_extent_create(ctx, directory_address, 12);
        if (new_extent_address == FS_NULL)
            return 4;
        
        directory.extent.next = new_extent_address;
        fs_directory_header_write(ctx, directory_address, &directory);
        
        if (!fs_directory_extent_read(ctx, new_extent_address, &extent))
            return 5;
        uint32_t ref_offset = new_extent_address + sizeof(struct FSDirectoryExtent);
        fs_mem_write(ctx, ref_offset, &reference_address, sizeof(uint32_t));
        extent.reference_count = 1;
        fs_directory_extent_write(ctx, new_extent_address, &extent);
        
        return 0;
    }
    
    if (!fs_directory_extent_read(ctx, extent_address, &tail_extent))
        return 6;
    new_extent_address = fs_directory_extent_create(ctx, extent_address, 12);
    if (new_extent_address == FS_NULL)
        return 7;
    
    tail_extent.extent.next = new_extent_address;
    fs_directory_extent_write(ctx, extent_address, &tail_extent);
    
    if (!fs_directory_extent_read(ctx, new_extent_address, &extent))
        return 8;
    uint32_t ref_offset = new_extent_address + sizeof(struct FSDirectoryExtent);
    fs_mem_write(ctx, ref_offset, &reference_address, sizeof(uint32_t));
    extent.reference_count = 1;
    fs_directory_extent_write(ctx, new_extent_address, &extent);
    
    return 0;
}

uint8_t fs_directory_remove_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t reference_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;
    uint32_t                 index;
    uint32_t                 shift_index;
    uint32_t                 current_ref;
    uint32_t                 next_ref;
    if (reference_address == FS_NULL)
        return 1;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return 2;
    for (index = 0; index < directory.reference_count; index++) {
        uint32_t ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (index * sizeof(uint32_t));
        fs_mem_read(ctx, ref_offset, &current_ref, sizeof(uint32_t));
        
        if (current_ref != reference_address)
            continue;
        for (shift_index = index; shift_index + 1UL < directory.reference_count; shift_index++) {
            uint32_t next_ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + ((shift_index + 1UL) * sizeof(uint32_t));
            uint32_t curr_ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (shift_index * sizeof(uint32_t));
            fs_mem_read(ctx, next_ref_offset, &next_ref, sizeof(uint32_t));
            fs_mem_write(ctx, curr_ref_offset, &next_ref, sizeof(uint32_t));
        }
        
        if (directory.reference_count > 0) {
            directory.reference_count--;
            uint32_t last_ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (directory.reference_count * sizeof(uint32_t));
            uint32_t null_ref = FS_NULL;
            fs_mem_write(ctx, last_ref_offset, &null_ref, sizeof(uint32_t));
        }
        
        fs_directory_header_write(ctx, directory_address, &directory);
        return 0;
    }
    
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            return 3;
        for (index = 0; index < extent.reference_count; index++) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (index * sizeof(uint32_t));
            fs_mem_read(ctx, ref_offset, &current_ref, sizeof(uint32_t));
            
            if (current_ref != reference_address)
                continue;
            for (shift_index = index; shift_index + 1UL < extent.reference_count; shift_index++) {
                uint32_t next_ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + ((shift_index + 1UL) * sizeof(uint32_t));
                uint32_t curr_ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (shift_index * sizeof(uint32_t));
                fs_mem_read(ctx, next_ref_offset, &next_ref, sizeof(uint32_t));
                fs_mem_write(ctx, curr_ref_offset, &next_ref, sizeof(uint32_t));
            }
            
            if (extent.reference_count > 0) {
                extent.reference_count--;
                uint32_t last_ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (extent.reference_count * sizeof(uint32_t));
                uint32_t null_ref = FS_NULL;
                fs_mem_write(ctx, last_ref_offset, &null_ref, sizeof(uint32_t));
            }
            
            if (extent.reference_count == 0) {
                fs_directory_extent_unlink_and_free(ctx, directory_address, extent_address);
            } else {
                fs_directory_extent_write(ctx, extent_address, &extent);
            }
            
            return 0;
        }
        
        extent_address = extent.extent.next;
    }
    
    return 4;
}

uint32_t fs_directory_get_reference(struct FSDeviceContext* ctx, uint32_t directory_address, uint32_t index) {
    uint32_t address = FS_NULL;
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return address;
    if (index < directory.reference_count) {
        uint32_t ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (index * sizeof(uint32_t));
        fs_mem_read(ctx, ref_offset, &address, sizeof(uint32_t));
        return address;
    }
    
    index -= directory.reference_count;
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            return address;
        if (index < extent.reference_count) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (index * sizeof(uint32_t));
            fs_mem_read(ctx, ref_offset, &address, sizeof(uint32_t));
            return address;
        }
        
        index -= extent.reference_count;
        extent_address = extent.extent.next;
    }
    
    return address;
}

uint32_t fs_directory_get_reference_count(struct FSDeviceContext* ctx, uint32_t directory_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;
    uint32_t                 total_count = 0;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return FS_NULL;
    
    total_count = (uint32_t)directory.reference_count;
    
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent)) 
            break;
        total_count += (uint32_t)extent.reference_count;
        extent_address = extent.extent.next;
    }
    return total_count;
}

uint32_t fs_directory_get_parent(struct FSDeviceContext* ctx, uint32_t directory_address) {
    struct FSDirectoryHeader directory;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return FS_NULL;
    
    return directory.parent;
}

uint32_t fs_directory_find(struct FSDeviceContext* ctx, uint32_t directory_address, const char* name) {
    struct FSDirectoryHeader   directory;
    struct FSDirectoryExtent   extent;
    struct FSBlockHeader object;
    
    uint32_t reference_address;
    uint32_t extent_address;
    uint32_t index;
    
    if (name == NULL)
        return FS_NULL;
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return FS_NULL;
    for (index = 0; index < directory.reference_count; index++) {
        uint32_t ref_offset = directory_address + sizeof(struct FSDirectoryHeader) + (index * sizeof(uint32_t));
        fs_mem_read(ctx, ref_offset, &reference_address, sizeof(uint32_t));
        
        if (reference_address == FS_NULL)
            continue;
        fs_mem_read(ctx, reference_address, &object, sizeof(struct FSBlockHeader));
        
        if (strncmp(object.name, name, sizeof(object.name)) == 0)
            return reference_address;
    }
    
    extent_address = directory.extent.next;
    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            return FS_NULL;
        for (index = 0; index < extent.reference_count; index++) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (index * sizeof(uint32_t));
            fs_mem_read(ctx, ref_offset, &reference_address, sizeof(uint32_t));
            
            if (reference_address == FS_NULL)
                continue;
            fs_mem_read(ctx, reference_address, &object, sizeof(struct FSBlockHeader));
            
            if (strncmp(object.name, name, sizeof(object.name)) == 0)
                return reference_address;
        }
        
        extent_address = extent.extent.next;
    }
    
    return FS_NULL;
}
