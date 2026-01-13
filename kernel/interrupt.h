#ifndef __KERNEL_INTERRUPT_
#define __KERNEL_INTERRUPT_

#include <stdint.h>

// Timer / Scheduler Controls
void InterruptStartTimeCounter(void);
void InterruptStartScheduler(void);
void InterruptStartHardware(void);

void InterruptStopTimeCounter(void);
void InterruptStopScheduler(void);
void InterruptStopHardware(void);

// Global Interrupts
void EnableGlobalInterrupts(void);
void DisableGlobalInterrupts(void);

// ISR Handler Registration
void SetTimerInterruptService(void (*service_ptr)(void));
void SetSchedulerInterruptService(void (*service_ptr)(void));

// Set the callback function for the hardware interrupt
void SetHardwareInterruptService(void (*service_ptr)(void));

#endif
