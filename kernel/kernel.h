#ifndef KERNEL_CORE_H
#define KERNEL_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>

#include <kernel/syscall/print.h>

#include <kernel/bus/bus.h>
#include <kernel/driver/driver.h>
#include <kernel/device/device.h>

uint32_t create_device(struct Device* device);

void destroy_device(uint32_t address);

void hardware_identify_devices(void);

uint32_t device_get_hardware_address(const char* name);

#endif
