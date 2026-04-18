#include <kernel/knode.h>
#include <string.h>
#include <stdint.h>

uint32_t create_knode(const char* name, uint32_t parent_address) {
    struct KernelDirectory kdir;
    memset(&kdir, 0x00, sizeof(struct KernelDirectory));
    
    strncpy(kdir.name, name, sizeof(kdir.name) - 1);
    kdir.name[sizeof(kdir.name) - 1] = '\0';
    
    kdir.parent = parent_address;
    kdir.next   = 0;
    kdir.prev   = 0;
    kdir.size   = 0;
    
    uint32_t knode_address = kmalloc(sizeof(struct KernelDirectory));
    if (knode_address == KMALLOC_NULL)
        return KMALLOC_NULL;
    
    kmalloc_set_flags(knode_address, KMALLOC_FLAG_DIRECTORY);
    kmalloc_set_permissions(knode_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmalloc_write(knode_address, &kdir, sizeof(struct KernelDirectory));
    
    knode_add_reference(parent_address, knode_address);
    return knode_address;
}

void destroy_knode(uint32_t address, uint32_t parent_address) {
    knode_remove_reference(parent_address, address);
    kfree(address);
}

uint32_t create_extent(void) {
    struct KernelDirectoryExtent kextent;
    memset(&kextent, 0x00, sizeof(struct KernelDirectoryExtent));
    kextent.next = 0;
    kextent.prev = 0;
    kextent.size = 0;
    kextent.reference[0] = 0;
    
    uint32_t device_address = kmalloc(sizeof(struct KernelDirectoryExtent));
    
    kmalloc_set_flags(device_address, KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_EXTENT);
    kmalloc_set_permissions(device_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmalloc_write(device_address, &kextent, sizeof(struct KernelDirectoryExtent));
    return device_address;
}

void destroy_extent(uint32_t address) {
    kfree(address);
}

static uint8_t knode_is_valid_address(uint32_t directory_address) {
    if (directory_address == KMALLOC_NULL || directory_address == 0) 
        return 0;
    
    uint8_t flags = kmalloc_get_flags(directory_address);
    
    if ((flags & KMALLOC_FLAG_DIRECTORY) == 0) 
        return 0;
    
    if ((flags & KMALLOC_FLAG_EXTENT) != 0) 
        return 0;
    
    return 1;
}
static uint8_t knode_contains_reference_address(uint32_t directory_address, uint32_t reference_ptr) {
    struct KernelDirectory directory;
    
    if (knode_is_valid_address(directory_address) == 0) 
        return 0;
    
    if (reference_ptr == 0 || reference_ptr == KMALLOC_NULL) 
        return 0;
    
    kmalloc_read(directory_address, &directory, sizeof(struct KernelDirectory));
    
    uint16_t reference_index;
    for (reference_index = 0; reference_index < directory.size; reference_index++) {
        if (directory.reference[reference_index] == reference_ptr) 
            return 1;
    }
    
    uint32_t current_extent_address = directory.next;
    
    while (current_extent_address != 0 && current_extent_address != KMALLOC_NULL) {
        struct KernelDirectoryExtent extent;
        kmalloc_read(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
        
        uint16_t reference_index;
        for (reference_index = 0; reference_index < extent.size; reference_index++) {
            if (extent.reference[reference_index] == reference_ptr) {
                return 1;
            }
        }
        
        current_extent_address = extent.next;
    }
    
    return 0;
}

uint8_t knode_add_reference(uint32_t directory_address, uint32_t reference_ptr) {
    if (knode_is_valid_address(directory_address) == 0)
        return 0;
    
    if (reference_ptr == 0 || reference_ptr == KMALLOC_NULL)
        return 0;
    
    if (knode_contains_reference_address(directory_address, reference_ptr) != 0)
        return 0;
    
    struct KernelDirectory directory;
    kmalloc_read(directory_address, &directory, sizeof(struct KernelDirectory));
    
    if (directory.size < KDIRECTORY_REF_MAX) {
        directory.reference[directory.size] = reference_ptr;
        directory.size++;
        kmalloc_write(directory_address, &directory, sizeof(struct KernelDirectory));
        return 1;
    }
    
    uint32_t current_extent_address  = directory.next;
    uint32_t previous_extent_address = 0;
    
    while (current_extent_address != 0 && current_extent_address != KMALLOC_NULL) {
        struct KernelDirectoryExtent extent;
        kmalloc_read(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
    
        if (extent.size < KDIRECTORYEX_REF_MAX) {
            extent.reference[extent.size] = reference_ptr;
            extent.size++;
            kmalloc_write(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
            return 1;
        }
    
        previous_extent_address = current_extent_address;
        current_extent_address  = extent.next;
    }
    
    uint32_t new_extent_address = create_extent();
    if (new_extent_address == KMALLOC_NULL)
        return 0;
    
    struct KernelDirectoryExtent new_extent;
    kmalloc_read(new_extent_address, &new_extent, sizeof(struct KernelDirectoryExtent));
    
    new_extent.prev         = previous_extent_address;
    new_extent.next         = 0;
    new_extent.size         = 1;
    new_extent.reference[0] = reference_ptr;
    
    kmalloc_write(new_extent_address, &new_extent, sizeof(struct KernelDirectoryExtent));
    
    if (previous_extent_address != 0) {
        struct KernelDirectoryExtent previous_extent;
        kmalloc_read(previous_extent_address, &previous_extent, sizeof(struct KernelDirectoryExtent));
        previous_extent.next = new_extent_address;
        kmalloc_write(previous_extent_address, &previous_extent, sizeof(struct KernelDirectoryExtent));
    } else {
        directory.next = new_extent_address;
    }
    
    directory.prev = new_extent_address;
    kmalloc_write(directory_address, &directory, sizeof(struct KernelDirectory));
    
    return 1;
}

uint8_t knode_remove_reference(uint32_t directory_address, uint32_t reference_ptr) {
    if (knode_is_valid_address(directory_address) == 0)
        return 0;
    
    struct KernelDirectory directory;
    kmalloc_read(directory_address, &directory, sizeof(struct KernelDirectory));
    
    uint16_t reference_index;
    for (reference_index = 0; reference_index < directory.size; reference_index++) {
        if (directory.reference[reference_index] == reference_ptr) {
            uint16_t shift_index;
            for (shift_index = reference_index; shift_index + 1 < directory.size; shift_index++) {
                directory.reference[shift_index] = directory.reference[shift_index + 1];
            }
            
            directory.size--;
            directory.reference[directory.size] = 0;
            
            kmalloc_write(directory_address, &directory, sizeof(struct KernelDirectory));
            return 1;
        }
    }
    
    uint32_t current_extent_address = directory.next;
    
    while (current_extent_address != 0 && current_extent_address != KMALLOC_NULL) {
        struct KernelDirectoryExtent extent;
        kmalloc_read(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
        
        for (reference_index = 0; reference_index < extent.size; reference_index++) {
            if (extent.reference[reference_index] == reference_ptr) {
                uint16_t shift_index;
                for (shift_index = reference_index; shift_index + 1 < extent.size; shift_index++) {
                    extent.reference[shift_index] = extent.reference[shift_index + 1];
                }
                
                extent.size--;
                extent.reference[extent.size] = 0;
                
                if (extent.size == 0) {
                    uint32_t previous_extent_address = extent.prev;
                    uint32_t next_extent_address     = extent.next;
                    
                    if (previous_extent_address != 0 && previous_extent_address != KMALLOC_NULL) {
                        struct KernelDirectoryExtent previous_extent;
                        kmalloc_read(previous_extent_address, &previous_extent, sizeof(struct KernelDirectoryExtent));
                        previous_extent.next = next_extent_address;
                        kmalloc_write(previous_extent_address, &previous_extent, sizeof(struct KernelDirectoryExtent));
                    } else {
                        directory.next = next_extent_address;
                    }
                    
                    if (next_extent_address != 0 && next_extent_address != KMALLOC_NULL) {
                        struct KernelDirectoryExtent next_extent;
                        kmalloc_read(next_extent_address, &next_extent, sizeof(struct KernelDirectoryExtent));
                        next_extent.prev = previous_extent_address;
                        kmalloc_write(next_extent_address, &next_extent, sizeof(struct KernelDirectoryExtent));
                    } else {
                        directory.prev = previous_extent_address;
                    }
                    
                    if (directory.next == 0 || directory.next == KMALLOC_NULL) {
                        directory.prev = 0;
                    }
                    
                    kmalloc_write(directory_address, &directory, sizeof(struct KernelDirectory));
                    destroy_extent(current_extent_address);
                } else {
                    kmalloc_write(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
                }
                
                return 1;
            }
        }
        
        current_extent_address = extent.next;
    }
    
    return 0;
}

uint32_t knode_get_reference(uint32_t directory_address, uint32_t index) {
    if (knode_is_valid_address(directory_address) == 0) 
        return KMALLOC_NULL;
    
    struct KernelDirectory directory;
    kmalloc_read(directory_address, &directory, sizeof(struct KernelDirectory));
    
    if (index < directory.size) 
        return directory.reference[index];
    
    index -= directory.size;
    uint32_t current_extent_address = directory.next;
    
    while (current_extent_address != 0) {
        struct KernelDirectoryExtent extent;
        kmalloc_read(current_extent_address, &extent, sizeof(struct KernelDirectoryExtent));
        
        if (index < extent.size) 
            return extent.reference[index];
        
        index -= extent.size;
        current_extent_address = extent.next;
    }
    return KMALLOC_NULL;
}

uint32_t knode_get_root(void) {
    uint32_t allocation_address = KMALLOC_NULL;
    uint32_t first_top_level    = KMALLOC_NULL;
    
    for (;;) {
        allocation_address = kmalloc_next(allocation_address);
        
        if (allocation_address == KMALLOC_NULL)
            break;
        
        uint8_t flags = kmalloc_get_flags(allocation_address);
        
        if ((flags & KMALLOC_FLAG_DIRECTORY) == 0)
            continue;
        
        if ((flags & KMALLOC_FLAG_EXTENT) != 0)
            continue;
        
        struct KernelDirectory directory;
        kmalloc_read(allocation_address, &directory, sizeof(struct KernelDirectory));
        
        if (directory.parent == 0 || directory.parent == KMALLOC_NULL) {
            if (strcmp(directory.name, "/") == 0)
                return allocation_address;
            
            if (first_top_level == KMALLOC_NULL)
                first_top_level = allocation_address;
        }
    }
    return first_top_level;
}

uint32_t knode_get_parent(uint32_t directory_address) {
    struct KernelDirectory directory;
    
    if (knode_is_valid_address(directory_address) == 0)
        return KMALLOC_NULL;
    
    kmalloc_read(directory_address, &directory, sizeof(struct KernelDirectory));
    
    if (directory.parent == 0 || directory.parent == KMALLOC_NULL)
        return directory_address;
    
    if (knode_is_valid_address(directory.parent) == 0)
        return directory_address;
    return directory.parent;
}

uint32_t knode_find_in(uint32_t directory_address, const char* name) {
    uint32_t reference_index = 0;
    
    if (knode_is_valid_address(directory_address) == 0) 
        return KMALLOC_NULL;
    
    if (name == NULL || name[0] == '\0') 
        return KMALLOC_NULL;
    
    while (1) {
        uint32_t reference_address =
            knode_get_reference(directory_address, reference_index);
        
        if (reference_address == KMALLOC_NULL || reference_address == 0) 
            break;
        
        char object_name[16];
        memset(object_name, 0x00, sizeof(object_name));
        
        kmalloc_read(reference_address, object_name, sizeof(object_name));
        
        if (strcmp(object_name, name) == 0) 
            return reference_address;
        
        reference_index++;
    }
    return KMALLOC_NULL;
}
