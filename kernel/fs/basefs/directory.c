
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

// Upper bound on any extent chain walk. A chain can never have more links
// than the device has blocks, so a longer walk means a corrupt (cyclic) chain.
static inline uint32_t fs_directory_walk_limit(struct FSDeviceContext* ctx) {
    return (ctx && ctx->block_count) ? ctx->block_count : 0xFFFFU;
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
    directory.block.name[sizeof(directory.block.name) - 1] = '\0';
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
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
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
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
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
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
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
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
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
    // Return 0 (not FS_NULL) on failure: callers use this as a loop bound
    if (!fs_directory_header_read(ctx, directory_address, &directory))
        return 0;
    
    total_count = (uint32_t)directory.reference_count;
    
    extent_address = directory.extent.next;
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
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

// Compare an entry's name the same way path lookup does: through
// fs_file_get_name, so names stored without a terminator still match.
static bool fs_directory_name_matches(struct FSDeviceContext* ctx, uint32_t address, const char* name) {
    char stored[FS_NAME_LENGTH_MAX];
    if (!fs_file_get_name(ctx, address, stored))
        return false;
    return strcmp(stored, name) == 0;
}

uint32_t fs_directory_find(struct FSDeviceContext* ctx, uint32_t directory_address, const char* name) {
    struct FSDirectoryHeader   directory;
    struct FSDirectoryExtent   extent;
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
        if (fs_directory_name_matches(ctx, reference_address, name))
            return reference_address;
    }
    
    extent_address = directory.extent.next;
    uint32_t walk_guard = fs_directory_walk_limit(ctx);
    while (extent_address != FS_NULL && walk_guard-- > 0) {
        if (!fs_directory_extent_read(ctx, extent_address, &extent))
            return FS_NULL;
        for (index = 0; index < extent.reference_count; index++) {
            uint32_t ref_offset = extent_address + sizeof(struct FSDirectoryExtent) + (index * sizeof(uint32_t));
            fs_mem_read(ctx, ref_offset, &reference_address, sizeof(uint32_t));
            
            if (reference_address == FS_NULL)
                continue;
            if (fs_directory_name_matches(ctx, reference_address, name))
                return reference_address;
        }
        
        extent_address = extent.extent.next;
    }
    
    return FS_NULL;
}




// True if `address` is a live allocation holding a directory header.
static bool fs_directory_is_directory(struct FSDeviceContext* ctx, uint32_t address) {
    struct FSDirectoryHeader directory;
    if (!fs_directory_header_read(ctx, address, &directory))
        return false;
    return (directory.block.attributes & FS_ATTRIBUTE_DIRECTORY) != 0;
}

// Delete a directory together with everything below it, then the directory
// itself. If `parent_directory` is not FS_NULL the directory is unlinked from
// it first, so an interrupted delete leaves orphaned blocks rather than a
// parent entry pointing at freed storage.
//
// The walk is iterative (no recursion on the kernel stack): it always works
// on the last entry of the current directory, descends into non-empty
// subdirectories, and climbs back up through the `parent` field once a
// directory has been emptied.
//
// Only directories whose `parent` field names the directory they were found
// in are descended into and freed. Anything else (a corrupt or cross-linked
// entry, or a link back to the directory being deleted) is only unlinked,
// never freed, so the walk cannot escape the subtree or loop forever.
//
// Permissions are not checked here; that is the caller's job.
// Refuses to delete the partition's root directory.
bool fs_directory_delete_recursive(struct FSDeviceContext* ctx, uint32_t address, uint32_t parent_directory) {
    if (!ctx || address == FS_NULL)
        return false;
    if (!fs_directory_is_directory(ctx, address))
        return false;
    
    struct FSPartitionBlock partition;
    if (fs_device_get_partition(ctx, &partition) != 0)
        return false;
    if (address == partition.root_directory)
        return false;
    
    if (parent_directory != FS_NULL)
        fs_directory_remove_reference(ctx, parent_directory, address);
    
    // Every step frees an allocation, unlinks an entry or descends once into
    // a directory that will later be freed, so a sane tree needs far fewer
    // steps than this. Running out means the structure is corrupt.
    uint32_t steps = fs_directory_walk_limit(ctx) * 4U;
    uint32_t current = address;
    
    while (steps-- > 0) {
        uint32_t count = fs_directory_get_reference_count(ctx, current);
        
        if (count == 0) {
            if (current == address) {
                fs_directory_delete(ctx, current);
                return true;
            }
            // Emptied a subdirectory: unlink it, free it, climb back up
            uint32_t up = fs_directory_get_parent(ctx, current);
            if (up == FS_NULL || fs_directory_remove_reference(ctx, up, current) != 0)
                return false;
            fs_directory_delete(ctx, current);
            current = up;
            continue;
        }
        
        uint32_t child = fs_directory_get_reference(ctx, current, count - 1U);
        
        if (child != FS_NULL && child != address && child != current &&
            fs_directory_is_directory(ctx, child) &&
            fs_directory_get_parent(ctx, child) == current) {
            // Owned subdirectory: empty it first (unlinked when we come back)
            current = child;
            continue;
        }
        
        // A file, a foreign/corrupt entry, or an empty slot: unlink it
        // (An FS_NULL slot inside the count can't be removed and would be
        // retried forever, so a failed unlink stops the walk.)
        if (fs_directory_remove_reference(ctx, current, child) != 0)
            return false;
        if (child != FS_NULL && fs_file_check(ctx, child))
            fs_file_delete(ctx, child);
    }
    
    return false;
}
