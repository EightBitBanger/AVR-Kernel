#ifndef BASE_IO_H
#define BASE_IO_H

#include <kernel/arch/avr/io.h>
#include <kernel/boot/avr/heap.h>

uint8_t fs_readb(uint32_t address);
void fs_writeb(uint32_t address, uint8_t byte);

void fs_mem_read(uint32_t address, void* destination, uint32_t size);
void fs_mem_write(uint32_t address, const void* source, uint32_t size);

#endif
