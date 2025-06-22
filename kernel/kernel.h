#ifndef __KERNEL_CORE_
#define __KERNEL_CORE_

#include <stdint.h>

#include <kernel/kconfig.h>
#include <kernel/version.h>

#include <kernel/kref.h>
#include <kernel/mutex.h>
#include <kernel/pointers.h>
#include <kernel/cstring.h>
#include <kernel/kalloc.h>
#include <kernel/throw.h>

#include <kernel/scheduler.h>
#include <kernel/interrupt.h>
#include <kernel/virtual/virtual.h>

#include <kernel/bus/bus.h>
#include <kernel/device/device.h>
#include <kernel/driver/driver.h>

#include <kernel/command/cli.h>

#include <kernel/drivers.h>

// File systems
#include <kernel/vfs.h>
#include <kernel/fs/fs.h>

// System subroutine calls
#include <kernel/syscalls/alloc/new.h>
#include <kernel/syscalls/print/print.h>
#include <kernel/syscalls/beep/beep.h>
#include <kernel/syscalls/timer/timer.h>
#include <kernel/syscalls/graphics/graphics.h>

// Firmware
#include <kernel/network/network.h>
#include <kernel/network/router.h>
#include <kernel/network/server.h>

// Emulators
#include <kernel/emulation/x4/x4.h>

#define nullptr   0x00000000
#define nullfunc  nullfunction

void nullfunction(void);

void kInit(void);

void KernelVectorTableInit(void);

void SetInterruptVector(uint8_t index, void(*servicePtr)());

void _ISR_hardware_service_routine(void);

#endif
