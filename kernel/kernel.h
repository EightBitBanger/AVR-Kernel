#ifndef KERNEL_CORE_H
#define KERNEL_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/arch/avr/io.h>
#include <kernel/arch/avr/map.h>

#include <kernel/boot/avr/heap.h>

#include <kernel/bus/bus.h>
#include <kernel/device/kbuffer.h>
#include <kernel/device/driver.h>
#include <kernel/device/device.h>
#include <kernel/device/knode.h>
#include <kernel/device/kevent.h>

#include <kernel/console/print.h>
#include <kernel/console/console.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/emulation/x4/x4.h>
#include <kernel/fs/fs.h>

#include <kernel/knode.h>
#include <kernel/syscall.h>


#define KSO_WORKING_DIRECTORY   0
#define KSO_LOCAL_PATHS         sizeof(struct WorkingDirectory)
//#define KSO_                    sizeof(struct WorkingDirectory) + sizeof(struct LocalPaths)


void kernel_init(void);

uint32_t create_device(const char* name);
void     destroy_device(uint32_t address);

uint32_t create_buffer(uint32_t size);
void     destroy_buffer(uint32_t address);

uint32_t create_driver(const char* name);
void     destroy_driver(uint32_t address);

uint32_t create_procblock(const char* name);
void     destroy_procblock(uint32_t address);


void kernel_get_system_object(void* object, uint32_t kso_sub_type, uint32_t kso_size);
void kernel_set_system_object(void* object, uint32_t kso_sub_type, uint32_t kso_size);

// Hardware identification

void hardware_identify_devices(uint32_t knode_device, uint32_t knode_mount, uint32_t nt_device);

uint32_t device_get_hardware_address(const char* name);

#endif
