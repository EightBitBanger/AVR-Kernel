#include <kernel/arch/x86/io.h>
#include <kernel/arch/x86/heap.h>
#include <kernel/arch/x86/bus/pci/pci.h>
#include <kernel/arch/x86/bus/pci/pci_internal.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/arch/x86/drivers/ahci/ahci.h>
#include <kernel/arch/x86/drivers/ata/ata.h>

#include <kernel/kernel.h>
#include <kernel/knode.h>
#include <kernel/fs/fs.h>

#include <kernel/console/print.h>
#include <kernel/util/string.h>

#define PCI_COMMAND_IO          (1U << 0)
#define PCI_COMMAND_MEMORY      (1U << 1)
#define PCI_COMMAND_BUS_MASTER  (1U << 2)

static uint16_t storage_ata_index = 0;
static uint16_t storage_ahci_index = 0;

const char* pci_get_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "legacy";
        case 0x01: return "storage";
        case 0x02: return "network";
        case 0x03: return "display";
        case 0x04: return "multimedia";
        case 0x06: return "bridge";
        case 0x0C: return "serialusb";
        default:   return "unknown";
    }
}

static void pci_setup_storage_paths(const char* device_name, struct LocalPaths* fs_paths) {
    char mount_root[64];
    memset(mount_root, '\0', sizeof(mount_root));
    strcat(mount_root, "/mnt/");
    strncat(mount_root, device_name, sizeof(mount_root) - strlen(mount_root) - 1);
    
    char bin_path[64];
    memset(bin_path, '\0', sizeof(bin_path));
    strncpy(bin_path, mount_root, sizeof(bin_path) - 1);
    strcat(bin_path, "/bin");
    
    if (vfs_directory_check(bin_path)) {
        strcat(bin_path, ";");
        strncpy(fs_paths->path, bin_path, PATH_LENGTH_MAX);
    }
    
    char sys_path[64];
    memset(sys_path, '\0', sizeof(sys_path));
    strncpy(sys_path, mount_root, sizeof(sys_path) - 1);
    
    if (vfs_directory_check(sys_path)) {
        strncpy(fs_paths->home, sys_path, PATH_LENGTH_MAX);
    }
}

static void pci_handle_ata(uint8_t bus, uint8_t dev, uint8_t func, uint8_t prog_if, uint32_t mnt_dir, struct LocalPaths* fs_paths) {
    uint16_t primary_io_base = (prog_if & 0x01) 
        ? pci_get_io_bar(bus, dev, func, 0) 
        : 0x1F0;
    
    if (primary_io_base == 0 || !ata_init(primary_io_base)) 
        return;
    
    char device_name[16] = "ata";
    
    char index_string[16];
    memset(index_string, '\0', 16);
    itos(storage_ata_index++, index_string);
    strncat(device_name, index_string, 16);
    
    uint32_t mount_ptr = create_knode(device_name, mnt_dir);
    kmalloc_set_flags(mount_ptr, (KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT));
    
    uint32_t block_device = (uint32_t)malloc(512);
    
    // Read sector 0 BEFORE opening the device, matching the AHCI path
    ata_read_sector(0, (uint8_t*)block_device);
    
    struct FSPartitionBlock part;
    struct FSDeviceContext context = fs_device_open(block_device, &part, FS_DEVICE_TYPE_ATA);
    
    uint32_t device_context = (uint32_t)malloc(sizeof(struct FSDeviceContext));
    memcpy((void*)device_context, &context, sizeof(struct FSDeviceContext));
    
    knode_add_reference(mount_ptr, block_device);
    knode_add_reference(mount_ptr, device_context);
    
    pci_setup_storage_paths(device_name, fs_paths);
    print("ATA device mounted\n");
}

static void pci_handle_ahci(uint8_t bus, uint8_t dev, uint8_t func, uint32_t mnt_dir, struct LocalPaths* fs_paths) {
    // Bus mastering + memory decode (already set by pci_handle_storage_device,
    // repeated here so this handler stands on its own)
    pci_enable_command_bits(bus, dev, func, PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER);
    
    // ABAR is BAR5. Map only the HBA register block, once. The bus scan no
    // longer maps BARs, so this is the only mapping of it. pci_map_bar()
    // handles 64-bit BARs and refuses addresses above 4 GB.
    uint32_t flags = VM_PRESENT | VM_READWRITE | VM_PCD; 
    struct AHCI_HBA_Memory_Space* ahci_base_vaddr = 
        (struct AHCI_HBA_Memory_Space*)pci_map_bar(bus, dev, func, 5, AHCI_HBA_MMIO_SIZE, flags); 
    if (ahci_base_vaddr == NULL) return;
    
    ahci_init(ahci_base_vaddr); 
    
    struct AHCI_Port_Registers* active_port = NULL; 
    for (int p = 0; p < 32; p++) { 
        if ((ahci_base_vaddr->ports_implemented & (1 << p)) && 
            (ahci_base_vaddr->ports[p].signature == AHCI_DEV_SATA)) { 
            active_port = &ahci_base_vaddr->ports[p]; 
            break; 
        }
    }
    
    if (active_port == NULL) return;
    
    char device_name[16] = "ahci";   // was sized to fit only "ahci" (5 bytes)
    
    char index_string[16];
    memset(index_string, '\0', 16);
    itos(storage_ahci_index, index_string);
    strncat(device_name, index_string, 16);
    
    uint32_t block_device = (uint32_t)vmm_alloc_pages(1);
    if (block_device == 0) {
        print("AHCI Error: Failed to allocate page-aligned block device buffer.\n");
        return;
    }
    
    if (ahci_read_sectors(active_port, 0, 1, (uint8_t*)block_device)) { 
        uint32_t mount_ptr = create_knode(device_name, mnt_dir); 
        kmalloc_set_flags(mount_ptr, (KMALLOC_FLAG_DIRECTORY | KMALLOC_FLAG_MOUNT)); 
        
        struct FSPartitionBlock part; 
        struct FSDeviceContext context = fs_device_open(block_device, &part, FS_DEVICE_TYPE_AHCI); 
        
        uint32_t device_context = (uint32_t)malloc(sizeof(struct FSDeviceContext)); 
        memcpy((void*)device_context, &context, sizeof(struct FSDeviceContext)); 
        
        knode_add_reference(mount_ptr, block_device); 
        knode_add_reference(mount_ptr, device_context); 
        
        pci_setup_storage_paths(device_name, fs_paths);
        
        storage_ahci_index++; 
        print("AHCI device mounted\n"); 
    } else {
        print("AHCI error - failed to read sector 0\n"); 
        vmm_free_pages((void*)block_device, 1);
    }
}

void pci_handle_storage_device(uint8_t bus, uint8_t dev, uint8_t func, uint8_t subclass, uint8_t prog_if, uint32_t mnt_dir, struct LocalPaths* fs_paths) {
    // Writes only the command half of the register, so pending (RW1C)
    // status bits are no longer cleared as a side effect
    pci_enable_command_bits(bus, dev, func, PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER);
    
    if (subclass == 0x01) {
        // IDE uses I/O port BARs (or the legacy 0x1F0 ports): enable I/O decode
        pci_enable_command_bits(bus, dev, func, PCI_COMMAND_IO);
        pci_handle_ata(bus, dev, func, prog_if, mnt_dir, fs_paths);
    } else if (subclass == 0x06) {
        pci_handle_ahci(bus, dev, func, mnt_dir, fs_paths);
    }
}

void pci_init(void) {
    uint32_t root_node = knode_get_root();
    uint32_t dev_directory = knode_find_by_name(root_node, "dev");
    uint32_t mnt_directory = knode_find_by_name(root_node, "mnt");
    uint32_t pci_directory = create_knode("pci", dev_directory);
    
    uint32_t bus0_directory = create_knode("bus0", pci_directory);
    
    knode_set_permissions(pci_directory, KMALLOC_PERMISSION_READ);
    knode_set_permissions(bus0_directory, KMALLOC_PERMISSION_READ);
    
    // Read the paths once, let every storage device on every bus update the
    // same copy, then publish it once. Previously each (recursive) bus scan
    // took its own copy and wrote it back on exit, so a parent bus finishing
    // after a bridge overwrote whatever the child bus had found.
    struct LocalPaths fs_paths;
    kernel_get_local_paths(&fs_paths);
    
    pci_scan_reset();
    pci_scan_bus(0, bus0_directory, mnt_directory, &fs_paths, 0);
    
    kernel_set_local_paths(&fs_paths);
}
