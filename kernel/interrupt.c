#include <avr/io.h>
#include <avr/interrupt.h>

#include <kernel/interrupt.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>

// Default empty ISR callback
static void DummyFunction(void) {}

// Timer/scheduler callback
void (*timer_callback)(void)     = DummyFunction;
void (*scheduler_callback)(void) = DummyFunction;

// Hardware interrupt handler
void (*interrupt_callback)(void) = DummyFunction;


// Global interrupts
void DisableGlobalInterrupts(void) {cli();}
void EnableGlobalInterrupts(void)  {sei();}

void InterruptStartScheduler(void) {
    // TIMER1, CTC mode, prescaler 64
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    TIMSK1 = (1 << OCIE1A);
    OCR1A  = 391;
}

void InterruptStartTimeCounter(void) {
    // TIMER0, CTC mode, prescaler 256
    TCCR0B = (1 << WGM02) | (1 << CS02);
    TIMSK0 = (1 << OCIE0A);
    OCR0A  = 98;
}

void InterruptStopScheduler(void) {
    TCCR1B = 0;   // Stop TIMER1
    TIMSK1 = 0;   // Disable its interrupts
}

void InterruptStopTimeCounter(void) {
    TCCR0B = 0;   // Stop TIMER0
    TIMSK0 = 0;   // Disable interrupts
}


void InterruptStartHardware(void) {
    EICRA &= ~((1 << ISC20) | (1 << ISC21));
    EICRA |=  (1 << ISC20);    // Falling edge trigger
    
    EIMSK |= (1 << INT2);      // Enable INT2
}

void InterruptStopHardware(void) {
    EIMSK &= ~(1 << INT2);     // Disable INT2
}

void SetTimerInterruptService(void (*service_ptr)(void)) {
    uint8_t sreg = SREG;
    cli();
    
    timer_callback = service_ptr;
    
    SREG = sreg;
    return;
}

void SetSchedulerInterruptService(void (*service_ptr)(void)) {
    uint8_t sreg = SREG;
    cli();
    
    scheduler_callback = service_ptr;
    
    SREG = sreg;
    return;
}

void SetHardwareInterruptService(void (*service_ptr)(void)) {
    uint8_t sreg = SREG;
    cli();
    
    interrupt_callback = service_ptr ? service_ptr : DummyFunction;
    
    SREG = sreg;
    return;
}

// ISR Definitions
ISR(TIMER0_COMPA_vect) { timer_callback(); }      // Timer
ISR(TIMER1_COMPA_vect) { scheduler_callback(); }  // Scheduler

ISR(INT2_vect) { interrupt_callback(); }          // System interrupt handler
