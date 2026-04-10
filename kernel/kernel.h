#ifndef KERNEL_CORE_H
#define KERNEL_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>

#include <kernel/syscall/print.h>
#include <kernel/syscall/console.h>

#include <kernel/bus/bus.h>
#include <kernel/device/driver.h>
#include <kernel/device/device.h>
#include <kernel/device/knode.h>

#include <kernel/knode.h>

void kernel_init(void);

uint32_t create_device(const char* name);
void     destroy_device(uint32_t address);

uint32_t create_driver(const char* name);
void     destroy_driver(uint32_t address);

void hardware_identify_devices(uint32_t knode_device, uint32_t knode_mount);

uint32_t device_get_hardware_address(const char* name);

#endif
