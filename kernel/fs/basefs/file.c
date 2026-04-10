#include <kernel/fs/basefs/file.h>

#include <string.h>
#include <stdbool.h>

extern uint8_t* fs_bitmap;
extern uint32_t fs_device_address;

static bool fs_file_address_valid(uint32_t address, uint32_t size) {
    if (address == FS_NULL)
        return false;

    if (fs_bitmap == NULL)
        return false;

    return true;
}

static bool fs_file_alloc_header_read_from_payload(uint32_t payload_address, struct FSAllocHeader* header) {
    uint32_t allocation_address;

    if (payload_address == FS_NULL)
        return false;

    if (payload_address < sizeof(struct FSAllocHeader))
        return false;

    allocation_address = payload_address - sizeof(struct FSAllocHeader);

    if (!fs_file_address_valid(allocation_address, sizeof(struct FSAllocHeader)))
        return false;

    fs_mem_read(allocation_address, header, sizeof(struct FSAllocHeader));

    if (header->size == 0)
        return false;

    return true;
}

static bool fs_directory_header_read(uint32_t directory_address, struct FSDirectoryHeader* directory) {
    struct FSAllocHeader alloc_header;

    if (!fs_file_alloc_header_read_from_payload(directory_address, &alloc_header))
        return false;

    if (alloc_header.size < sizeof(struct FSDirectoryHeader))
        return false;

    if (!fs_file_address_valid(directory_address, sizeof(struct FSDirectoryHeader)))
        return false;

    fs_mem_read(directory_address, directory, sizeof(struct FSDirectoryHeader));
    return true;
}

static void fs_directory_header_write(uint32_t directory_address, const struct FSDirectoryHeader* directory) {
    fs_mem_write(directory_address, directory, sizeof(struct FSDirectoryHeader));
}

static bool fs_directory_extent_read(uint32_t extent_address, struct FSDirectoryExtent* extent) {
    struct FSAllocHeader alloc_header;

    if (!fs_file_alloc_header_read_from_payload(extent_address, &alloc_header))
        return false;

    if (alloc_header.size < sizeof(struct FSDirectoryExtent))
        return false;

    if (!fs_file_address_valid(extent_address, sizeof(struct FSDirectoryExtent)))
        return false;

    fs_mem_read(extent_address, extent, sizeof(struct FSDirectoryExtent));
    return true;
}

static void fs_directory_extent_write(uint32_t extent_address, const struct FSDirectoryExtent* extent) {
    fs_mem_write(extent_address, extent, sizeof(struct FSDirectoryExtent));
}

static uint32_t fs_directory_extent_create(uint32_t prev_address) {
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;

    extent_address = fs_alloc(sizeof(struct FSDirectoryExtent));
    if (extent_address == FS_NULL)
        return FS_NULL;

    memset(&extent, 0x00, sizeof(struct FSDirectoryExtent));
    extent.prev = prev_address;
    extent.next = FS_NULL;
    extent.reference_count = 0;

    fs_directory_extent_write(extent_address, &extent);

    return extent_address;
}

static void fs_directory_extent_unlink_and_free(uint32_t directory_address, uint32_t extent_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    struct FSDirectoryExtent prev_extent;
    struct FSDirectoryExtent next_extent;

    if (!fs_directory_header_read(directory_address, &directory))
        return;

    if (!fs_directory_extent_read(extent_address, &extent))
        return;

    if (extent.prev == directory_address) {
        directory.next = extent.next;
        fs_directory_header_write(directory_address, &directory);
    } else if (extent.prev != FS_NULL) {
        if (fs_directory_extent_read(extent.prev, &prev_extent)) {
            prev_extent.next = extent.next;
            fs_directory_extent_write(extent.prev, &prev_extent);
        }
    }

    if (extent.next != FS_NULL) {
        if (fs_directory_extent_read(extent.next, &next_extent)) {
            next_extent.prev = extent.prev;
            fs_directory_extent_write(extent.next, &next_extent);
        }
    }

    fs_free(extent_address);
}

uint32_t fs_file_create(const char* name, uint8_t permissions) {
    struct FSFileHeader file;
    uint32_t            address;

    address = fs_alloc(sizeof(struct FSFileHeader));
    if (address == FS_NULL)
        return FS_NULL;

    memset(&file, 0x00, sizeof(struct FSFileHeader));
    strncpy(file.name, name, sizeof(file.name) - 1);
    file.attributes  = 0;
    file.permissions = permissions;

    fs_mem_write(address, &file, sizeof(struct FSFileHeader));

    return address;
}

void fs_file_delete(uint32_t address) {
    fs_free(address);
}

uint32_t fs_directory_create(const char* name, uint8_t permissions) {
    struct FSDirectoryHeader directory;
    uint32_t                 address;

    address = fs_alloc(sizeof(struct FSDirectoryHeader));
    if (address == FS_NULL)
        return FS_NULL;

    memset(&directory, 0x00, sizeof(struct FSDirectoryHeader));
    strncpy(directory.name, name, sizeof(directory.name) - 1);
    directory.attributes      = FS_ATTRIBUTE_DIRECTORY;
    directory.permissions     = permissions;
    directory.parent          = FS_NULL;
    directory.next            = FS_NULL;
    directory.prev            = FS_NULL;
    directory.reference_count = 0;

    fs_directory_header_write(address, &directory);

    return address;
}

void fs_directory_delete(uint32_t address) {
    struct FSDirectoryHeader directory;
    uint32_t                 extent_address;
    uint32_t                 next_extent_address;

    if (!fs_directory_header_read(address, &directory))
        return;

    extent_address = directory.next;

    while (extent_address != FS_NULL) {
        struct FSDirectoryExtent extent;

        if (!fs_directory_extent_read(extent_address, &extent))
            break;

        next_extent_address = extent.next;
        fs_free(extent_address);
        extent_address = next_extent_address;
    }

    fs_free(address);
}

uint8_t fs_directory_add_reference(uint32_t directory_address, uint32_t reference_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    struct FSDirectoryExtent tail_extent;
    uint32_t                 extent_address;
    uint32_t                 new_extent_address;
    uint32_t                 index;

    if (reference_address == FS_NULL)
        return 1;

    if (!fs_directory_header_read(directory_address, &directory))
        return 2;

    for (index = 0; index < directory.reference_count; index++) {
        if (directory.reference[index] == reference_address)
            return 0;
    }

    if (directory.reference_count < 32UL) {
        directory.reference[directory.reference_count] = reference_address;
        directory.reference_count++;
        fs_directory_header_write(directory_address, &directory);
        return 0;
    }

    extent_address = directory.next;

    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(extent_address, &extent))
            return 3;

        for (index = 0; index < extent.reference_count; index++) {
            if (extent.reference[index] == reference_address)
                return 0;
        }

        if (extent.reference_count < 32UL) {
            extent.reference[extent.reference_count] = reference_address;
            extent.reference_count++;
            fs_directory_extent_write(extent_address, &extent);
            return 0;
        }

        if (extent.next == FS_NULL)
            break;

        extent_address = extent.next;
    }

    if (directory.next == FS_NULL) {
        new_extent_address = fs_directory_extent_create(directory_address);
        if (new_extent_address == FS_NULL)
            return 4;

        directory.next = new_extent_address;
        fs_directory_header_write(directory_address, &directory);

        if (!fs_directory_extent_read(new_extent_address, &extent))
            return 5;

        extent.reference[0] = reference_address;
        extent.reference_count = 1;
        fs_directory_extent_write(new_extent_address, &extent);

        return 0;
    }

    if (!fs_directory_extent_read(extent_address, &tail_extent))
        return 6;

    new_extent_address = fs_directory_extent_create(extent_address);
    if (new_extent_address == FS_NULL)
        return 7;

    tail_extent.next = new_extent_address;
    fs_directory_extent_write(extent_address, &tail_extent);

    if (!fs_directory_extent_read(new_extent_address, &extent))
        return 8;

    extent.reference[0] = reference_address;
    extent.reference_count = 1;
    fs_directory_extent_write(new_extent_address, &extent);

    return 0;
}

uint8_t fs_directory_remove_reference(uint32_t directory_address, uint32_t reference_address) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;
    uint32_t                 index;
    uint32_t                 shift_index;

    if (reference_address == FS_NULL)
        return 1;

    if (!fs_directory_header_read(directory_address, &directory))
        return 2;

    for (index = 0; index < directory.reference_count; index++) {
        if (directory.reference[index] != reference_address)
            continue;

        for (shift_index = index; shift_index + 1UL < directory.reference_count; shift_index++) {
            directory.reference[shift_index] = directory.reference[shift_index + 1UL];
        }

        if (directory.reference_count > 0) {
            directory.reference_count--;
            directory.reference[directory.reference_count] = FS_NULL;
        }

        fs_directory_header_write(directory_address, &directory);
        return 0;
    }

    extent_address = directory.next;

    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(extent_address, &extent))
            return 3;

        for (index = 0; index < extent.reference_count; index++) {
            if (extent.reference[index] != reference_address)
                continue;

            for (shift_index = index; shift_index + 1UL < extent.reference_count; shift_index++) {
                extent.reference[shift_index] = extent.reference[shift_index + 1UL];
            }

            if (extent.reference_count > 0) {
                extent.reference_count--;
                extent.reference[extent.reference_count] = FS_NULL;
            }

            if (extent.reference_count == 0) {
                fs_directory_extent_unlink_and_free(directory_address, extent_address);
            } else {
                fs_directory_extent_write(extent_address, &extent);
            }

            return 0;
        }

        extent_address = extent.next;
    }

    return 4;
}

uint32_t fs_directory_get_reference(uint32_t directory_address, uint32_t index) {
    struct FSDirectoryHeader directory;
    struct FSDirectoryExtent extent;
    uint32_t                 extent_address;

    if (!fs_directory_header_read(directory_address, &directory))
        return FS_NULL;

    if (index < directory.reference_count)
        return directory.reference[index];

    index -= directory.reference_count;
    extent_address = directory.next;

    while (extent_address != FS_NULL) {
        if (!fs_directory_extent_read(extent_address, &extent))
            return FS_NULL;

        if (index < extent.reference_count)
            return extent.reference[index];

        index -= extent.reference_count;
        extent_address = extent.next;
    }

    return FS_NULL;
}
