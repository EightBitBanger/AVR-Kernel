#ifndef BASE_IO_H
#define BASE_IO_H

#define FS_DEVICE_TYPE_ATA         0x0000
#define FS_DEVICE_TYPE_AHCI        0x0001

#ifdef KERNEL_PLATFORM_AVR
  #include <kernel/arch/avr/io.h>
  #include <kernel/arch/avr/heap.h>
#endif

#ifdef KERNEL_PLATFORM_X86
  #include <kernel/arch/x86/heap.h>
#endif

uint8_t fs_readb(uint32_t address);
void fs_writeb(uint32_t address, uint8_t byte);

void fs_mem_read(uint32_t address, void* destination, uint32_t size);
void fs_mem_write(uint32_t address, const void* source, uint32_t size);

#endif
