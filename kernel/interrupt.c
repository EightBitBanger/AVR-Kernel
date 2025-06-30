#include <avr/io.h>
#include <avr/interrupt.h>

#include <kernel/interrupt.h>
#include <kernel/kernel.h>

#include <kernel/mutex.h>

void dummyFunction(void) {};

void (*_timer_comp_a_ptr__)() = dummyFunction;
void (*_timer_comp_b_ptr__)() = dummyFunction;
void (*_timer_comp_c_ptr__)() = dummyFunction;

void (*_external_int_ptr__)() = dummyFunction;


void DisableGlobalInterrupts(void) {
    cli();
}

void EnableGlobalInterrupts(void) {
    sei();
}

void InterruptStartScheduler(void) {
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  // Set CTC mode, prescaler = 64
    TIMSK1 = (1 << OCIE1A);                             // Enable interrupt on compare match A
    OCR1A = 391;
}

void InterruptStartTimeCounter(void) {
    TCCR0B = (1 << WGM02) | (1 << CS02);  // CTC mode, prescaler = 256
    TIMSK0 = (1 << OCIE0A);               // Enable interrupt on compare match A
    OCR0A = 98;
}

void InterruptStartHardware(void) {
    EICRA = (1 << ISC20);   // Trigger on the falling edge
    //EICRA = (1 << ISC21); // Rising edge
    EIMSK |= (1 << INT2);   // Enable on pin PB2
}

void InterruptStopScheduler(void) {
    TCCR0B  = 0;
}

void InterruptStopTimerCounter(void) {
    TCCR1B  = 0;
	TCCR2B  = 0;
}

void InterruptStopHardware(void) {
    EICRA = 0;
    EIMSK = 0;
}



//
// Interrupts
//

uint8_t SetInterruptService(uint8_t index, void (*service_ptr)()) {
    switch (index) {
    case 0: _timer_comp_a_ptr__ = service_ptr;
    case 1: _timer_comp_b_ptr__ = service_ptr;
    case 2: _timer_comp_c_ptr__ = service_ptr;
    }
    return 1;
}

uint8_t SetHardwareInterruptService(void (*service_ptr)()) {
    _external_int_ptr__ = service_ptr;
    return 1;
}

//
// Interrupt service routines
//

ISR (TIMER0_COMPA_vect) {
    _timer_comp_a_ptr__();
}

ISR (TIMER1_COMPA_vect) {
    _timer_comp_b_ptr__();
}

ISR (TIMER2_COMPA_vect) {
    _timer_comp_c_ptr__();
}

ISR (INT2_vect) {
    _external_int_ptr__();
}
