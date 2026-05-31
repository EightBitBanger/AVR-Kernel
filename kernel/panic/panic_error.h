#ifndef KERNEL_PANIC_HANDLER_H
#define KERNEL_PANIC_HANDLER_H

#include <stdint.h>

#include <kernel/arch/x86/drivers/draw.h>
#include <kernel/console/display.h>
#include <kernel/console/print.h>

// Define the structure of the page fault error code bits
#define PF_PRESENT  (1 << 0) // 0: Super non-present page, 1: Page-protection violation
#define PF_WRITE    (1 << 1) // 0: Read access, 1: Write access
#define PF_USER     (1 << 2) // 0: Kernel-mode fault, 1: User-mode fault
#define PF_RESERVED (1 << 3) // 1: Overwrote a reserved bit in the page table
#define PF_INSTRUCT (1 << 4) // 1: Fault occurred during an instruction fetch

void kernel_panic_screen(uint32_t error_code, uint32_t faulting_address);

#endif
