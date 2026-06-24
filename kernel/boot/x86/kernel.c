#include <kernel/util/string.h>
#include <stdint.h>
#include <stddef.h>

#include <kernel/kernel.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>

extern uint32_t k_system_object;


void kernel_init(void) {
    k_system_object = kmalloc(1024 * 4);
    
    uint32_t root_node = create_knode("", 0);
    knode_set_root(root_node);
    
    struct WorkingDirectory fs_current;
    struct LocalPaths fs_paths;
    fs_current.current_directory = root_node;
    fs_current.mount_device      = FS_NULL;
    fs_current.mount_directory   = FS_NULL;
    fs_current.mount_root        = FS_NULL;
    
    uint32_t proc = create_knode("proc", root_node);
    uint32_t dev  = create_knode("dev", root_node);
    uint32_t mnt  = create_knode("mnt", root_node);
    
    knode_set_permissions(proc, KMALLOC_PERMISSION_READ);
    knode_set_permissions(dev,  KMALLOC_PERMISSION_READ);
    knode_set_permissions(mnt,  KMALLOC_PERMISSION_READ);
    
    hardware_identify_devices(dev, mnt, dev);
    
    kernel_set_local_paths(&fs_paths);
    kernel_set_working_directory(&fs_current);
}

void hardware_identify_devices(uint32_t knode_device, uint32_t knode_mount, uint32_t nt_device) {
    
}

uint32_t device_get_hardware_address(const char* name) {
    return 0;
}

void device_get_hardware_data(uint32_t address, char* data_buffer, uint8_t size) {
    
}
