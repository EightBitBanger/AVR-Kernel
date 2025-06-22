#ifndef __KERNEL_INTERRUPT_
#define __KERNEL_INTERRUPT_

// Timers / counters

void InterruptStartTimeCounter(void);
void InterruptStartScheduler(void);
void InterruptStartHardware(void);

void InterruptStopTimerCounter(void);
void InterruptStopScheduler(void);
void InterruptStopHardware(void);


// Interrupts

void EnableGlobalInterrupts(void);
void DisableGlobalInterrupts(void);


// Interrupt service routines

uint8_t SetInterruptService(uint8_t index, void (*service_ptr)());

// Hardware interrupt service routines

uint8_t SetHardwareInterruptService(void (*service_ptr)());

#endif
