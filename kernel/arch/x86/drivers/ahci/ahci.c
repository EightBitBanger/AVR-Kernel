#include <kernel/arch/x86/drivers/ahci/ahci.h>
#include <kernel/arch/x86/virtual/pmm.h>
#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/console/print.h>
#include <kernel/util/string.h>

// Global virtual mapping addresses so the CPU can configure the structures
static uint8_t* global_cmd_list_vaddr = NULL;
static uint8_t* global_fis_recv_vaddr = NULL;
static uint8_t* global_cmd_table_vaddr = NULL;

// Store the direct physical addresses to hand off over to the AHCI registers
static uint32_t global_cmd_list_phys = 0;
static uint32_t global_cmd_table_phys = 0;

static struct AHCI_HBA_Memory_Space* global_hba = NULL;

static void ahci_start_port(struct AHCI_Port_Registers* port) {
    // Wait until the controller command engine is completely idle
    uint32_t timeout = 1000000;
    while ((port->command_and_status & (1 << 15)) && timeout > 0) {
        timeout--;
    }
    
    // Enable FIS Receive FIRST so the HBA can communicate with the drive
    port->command_and_status |= (1 << 4);  // Set FRE (FIS Receive Enable)
    
    // NOW it is safe to wait for the drive to drop BSY and DRQ
    timeout = 1000000;
    while ((port->task_file_data & (0x80 | 0x08)) && timeout > 0) {
        timeout--;
    }
    
    // Finally, start processing commands
    port->command_and_status |= (1 << 0);  // Set ST (Start Processing)
}

static void ahci_stop_port(struct AHCI_Port_Registers* port) {
    // Clear ST (Start) and FRE (FIS Receive Enable)
    port->command_and_status &= ~(1 << 0);
    port->command_and_status &= ~(1 << 4);
    
    // Wait for CR (bit 15) and FR (bit 14) to clear with a timeout loop
    uint32_t timeout = 1000000;
    while (timeout > 0) {
        if (!(port->command_and_status & (1 << 15)) && 
            !(port->command_and_status & (1 << 14))) {
            break; // Engine stopped successfully!
        }
        timeout--;
    }
}

void ahci_init(void* abar_virtual_address) {
    // Store the virtual address globally so other functions can reference it
    global_hba = (struct AHCI_HBA_Memory_Space*)abar_virtual_address;
    
    // Force AHCI Mode Enable
    global_hba->global_host_control |= (1U << 31);
    
    uint32_t active_ports = global_hba->ports_implemented;
    for (int i = 0; i < 32; i++) {
        if (active_ports & (1 << i)) {
            struct AHCI_Port_Registers* port = &global_hba->ports[i];
            
            uint32_t status = port->sata_status;
            uint8_t ipm = (status >> 8) & 0x0F;
            uint8_t det = status & 0x0F;
            
            if (det == 0x03 && ipm == 0x01) {
                if (port->signature == AHCI_DEV_SATA) {
                    ahci_stop_port(port);
                    
                    // Allocate exact physical frames directly from the PMM!
                    global_cmd_list_phys  = pmm_alloc_frame();
                    uint32_t fis_recv_phys = pmm_alloc_frame();
                    global_cmd_table_phys = pmm_alloc_frame();
                    
                    // Map these physical frames into the dynamic virtual memory space
                    global_cmd_list_vaddr  = (uint8_t*)vmm_alloc_pages(1);
                    global_fis_recv_vaddr  = (uint8_t*)vmm_alloc_pages(1);
                    global_cmd_table_vaddr = (uint8_t*)vmm_alloc_pages(1);
                    
                    // Remap the allocated virtual addresses to point to our exact physical frames
                    vmm_map_page(global_cmd_list_phys, (uint32_t)global_cmd_list_vaddr, VM_PRESENT | VM_READWRITE);
                    vmm_map_page(fis_recv_phys, (uint32_t)global_fis_recv_vaddr, VM_PRESENT | VM_READWRITE);
                    vmm_map_page(global_cmd_table_phys, (uint32_t)global_cmd_table_vaddr, VM_PRESENT | VM_READWRITE);
                    
                    memset(global_cmd_list_vaddr, 0, 4096);
                    memset(global_fis_recv_vaddr, 0, 4096);
                    memset(global_cmd_table_vaddr, 0, 4096);
                    
                    // Hand the raw physical addresses right to the controller
                    port->command_list_base = global_cmd_list_phys;
                    port->fis_base          = fis_recv_phys;
                    
                    ahci_start_port(port);
                }
            }
        }
    }
}

// Get a port by its index
struct AHCI_Port_Registers* ahci_get_port(int port_num) {
    // Ensure the controller has been initialized
    if (global_hba == NULL) return NULL;
    
    // Validate bounds (AHCI supports a max of 32 ports)
    if (port_num < 0 || port_num >= 32) return NULL;
    
    // Check if the hardware actually implements this port
    if (!(global_hba->ports_implemented & (1 << port_num))) {
        return NULL; 
    }
    
    return &global_hba->ports[port_num];
}

static int ahci_find_free_cmd_slot(struct AHCI_Port_Registers* port) {
    uint32_t slots = (port->sata_active | port->command_issue);
    for (int i = 0; i < 32; i++) {
        if ((slots & (1 << i)) == 0) return i;
    }
    return -1;
}

bool ahci_read_sectors(struct AHCI_Port_Registers* port, uint64_t start_lba, uint32_t count, uint8_t* virtual_buffer) {
    int slot = ahci_find_free_cmd_slot(port);
    if (slot == -1) return false;
    
    // Clear pending interrupt status indicators before issuing new transactions
    port->interrupt_status = 0xFFFFFFFF;
    
    // DYNAMICALLY calculate virtual pointer space from physical base configuration registers
    // This replaces relying on global variables that get overwritten!
    uint32_t cmd_list_phys = port->command_list_base;
    struct AHCI_Command_Header* cmd_header = (struct AHCI_Command_Header*)vmm_get_virt_addr(cmd_list_phys);
    cmd_header += slot;
    
    cmd_header->fis_length = 5;
    cmd_header->write = 0;    
    cmd_header->prdt_length = 1;
    
    // Use the exact command table physical base allocated for this sequence
    uint32_t cmd_table_phys = cmd_header->command_table_base_addr;
    struct AHCI_Command_Table* cmd_table = (struct AHCI_Command_Table*)vmm_get_virt_addr(cmd_table_phys);
    memset((void*)cmd_table, 0, 4096);
    
    uint32_t buffer_phys = vmm_get_phys_addr(virtual_buffer);
    cmd_table->prdt_entries[0].data_base_addr = buffer_phys;
    cmd_table->prdt_entries[0].byte_count = (count * 512) - 1;
    cmd_table->prdt_entries[0].interrupt = 1;
    
    uint8_t* fis = cmd_table->command_fis;
    fis[0] = 0x27;      
    fis[1] = 0x80;      
    fis[2] = ATA_CMD_READ_DMA_EX;
    
    fis[4] = (uint8_t)(start_lba & 0xFF);
    fis[5] = (uint8_t)((start_lba >> 8) & 0xFF);
    fis[6] = (uint8_t)((start_lba >> 16) & 0xFF);
    fis[7] = 0x40;      
    
    fis[8] = (uint8_t)((start_lba >> 24) & 0xFF);
    fis[9] = (uint8_t)((start_lba >> 32) & 0xFF);
    fis[10] = (uint8_t)((start_lba >> 40) & 0xFF);
    
    fis[12] = (uint8_t)(count & 0xFF);
    fis[13] = (uint8_t)((count >> 8) & 0xFF);
    
    // Fire command execution
    port->command_issue = (1 << slot);
    
    uint32_t timeout = 5000000;
    while (timeout > 0) {
        if (!(port->command_issue & (1 << slot))) {
            break;
        }
        
        if (port->task_file_data & 0x01) {
            print("AHCI Error: Task file error status detected.\n");
            return false;
        }
        
        timeout--;
    }
    
    if (timeout == 0) {
        print("AHCI Error: Read operation timed out.\n");
        return false;
    }
    
    return (port->sata_error == 0);
}

bool ahci_write_sectors(struct AHCI_Port_Registers* port, uint64_t start_lba, uint32_t count, const uint8_t* virtual_buffer) {
    int slot = ahci_find_free_cmd_slot(port);
    if (slot == -1) return false;
    
    struct AHCI_Command_Header* cmd_header = (struct AHCI_Command_Header*)global_cmd_list_vaddr;
    cmd_header += slot;
    
    cmd_header->fis_length = 5;
    cmd_header->write = 1;      
    cmd_header->prdt_length = 1;
    
    cmd_header->command_table_base_addr = global_cmd_table_phys;
    
    struct AHCI_Command_Table* cmd_table = (struct AHCI_Command_Table*)global_cmd_table_vaddr;
    memset(global_cmd_table_vaddr, 0, 4096);
    
    uint32_t buffer_phys = vmm_get_phys_addr((void*)virtual_buffer);
    cmd_table->prdt_entries[0].data_base_addr = buffer_phys;
    cmd_table->prdt_entries[0].byte_count = (count * 512) - 1;
    cmd_table->prdt_entries[0].interrupt = 1;
    
    uint8_t* fis = cmd_table->command_fis;
    fis[0] = 0x27;
    fis[1] = 0x80;
    fis[2] = ATA_CMD_WRITE_DMA_EX; 
    
    fis[4] = (uint8_t)(start_lba & 0xFF);
    fis[5] = (uint8_t)((start_lba >> 8) & 0xFF);
    fis[6] = (uint8_t)((start_lba >> 16) & 0xFF);
    fis[7] = 0x40;
    
    fis[8] = (uint8_t)((start_lba >> 24) & 0xFF);
    fis[9] = (uint8_t)((start_lba >> 32) & 0xFF);
    fis[10] = (uint8_t)((start_lba >> 40) & 0xFF);
    
    fis[12] = (uint8_t)(count & 0xFF);
    fis[13] = (uint8_t)((count >> 8) & 0xFF);
    
    port->command_issue = (1 << slot);
    
    // Non-aggressive polling loop with timeout & error checks
    uint32_t timeout = 5000000; 
    while (timeout > 0) {
        if (!(port->command_issue & (1 << slot))) {
            break;
        }
        if (port->task_file_data & 0x01) {
            print("AHCI Error: Task file error status detected.\n");
            while(1);
            return false;
        }
        timeout--;
    }
    
    if (timeout == 0) {
        print("AHCI Error: Write operation timed out.\n");
        while(1);
        return false;
    }
    
    return (port->sata_error == 0);
}
