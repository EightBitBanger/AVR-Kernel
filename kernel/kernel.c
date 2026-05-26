#include <stdint.h>
#include <stddef.h>

#include <kernel/kernel.h>

#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/util/string.h>

struct KernelSystemObject {
    
    struct WorkingDirectory fs_current;
    struct LocalPaths paths;
    
};
uint32_t k_system_object;


void kernel_get_working_directory(struct WorkingDirectory* out_dir) {
    kmem_read((struct WorkingDirectory*)out_dir, 
               k_system_object + offsetof(struct KernelSystemObject, fs_current), 
               sizeof(struct WorkingDirectory));
}

void kernel_get_local_paths(struct LocalPaths* out_paths) {
    kmem_read((struct LocalPaths*)out_paths, 
               k_system_object + offsetof(struct KernelSystemObject, paths), 
               sizeof(struct LocalPaths));
}

void kernel_set_working_directory(struct WorkingDirectory* out_dir) {
    kmem_write(k_system_object + offsetof(struct KernelSystemObject, fs_current), 
               (struct WorkingDirectory*)out_dir, 
               sizeof(struct WorkingDirectory));
}

void kernel_set_local_paths(struct LocalPaths* out_paths) {
    kmem_write(k_system_object + offsetof(struct KernelSystemObject, paths), 
               (struct LocalPaths*)out_paths, 
               sizeof(struct LocalPaths));
}



uint32_t create_device(const char* name) {
    struct Device device;
    memset(&device, 0x00, sizeof(struct Device));
    strcpy(device.name, name);
    
    uint32_t device_address = kmalloc(sizeof(struct Device));
    
    kmalloc_set_type(device_address, KMALLOC_TYPE_DEVICE);
    kmalloc_set_permissions(device_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmem_write(device_address, &device, sizeof(struct Device));
    return device_address;
}

void destroy_device(uint32_t address) {
    kfree(address);
}

uint32_t create_procblock(const char* name) {
    struct ProcessBlock proc;
    memset(&proc, 0x00, sizeof(struct ProcessBlock));
    strcpy(proc.name, name);
    
    uint32_t proc_address = kmalloc(sizeof(struct ProcessBlock));
    
    kmalloc_set_type(proc_address, KMALLOC_TYPE_PROCBLOCK);
    kmalloc_set_permissions(proc_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmem_write(proc_address, &proc, sizeof(struct ProcessBlock));
    return proc_address;
}

void destroy_procblock(uint32_t address) {
    kfree(address);
}

uint32_t create_buffer(uint32_t size) {
    uint8_t buffer[size];
    memset(&buffer, 0x00, size);
    
    uint32_t buffer_address = kmalloc(size);
    kmalloc_set_type(buffer_address, KMALLOC_TYPE_DEVICE);
    
    kmem_write(buffer_address, &buffer, size);
    return buffer_address;
}

void destroy_buffer(uint32_t address) {
    struct KMallocHeader header;
    kmem_read(&header, address - sizeof(struct KMallocHeader), sizeof(struct KMallocHeader));
    
    uint32_t size = header.size;
    uint8_t buffer[size];
    memset(&buffer, 0x00, size);
    
    kmem_write(address, &buffer, size);
    kfree(address);
}

uint32_t create_driver(const char* name) {
    struct Driver driver;
    memset(&driver, 0x00, sizeof(struct Driver));
    strcpy(driver.name, name);
    
    uint32_t driver_address = kmalloc(sizeof(struct Driver));
    
    kmalloc_set_type(driver_address, KMALLOC_TYPE_DRIVER);
    kmalloc_set_permissions(driver_address, KMALLOC_PERMISSION_READ | KMALLOC_PERMISSION_WRITE);
    
    kmem_write(driver_address, &driver, sizeof(struct Driver));
    return driver_address;
}

void destroy_driver(uint32_t address) {
    kfree(address);
}
