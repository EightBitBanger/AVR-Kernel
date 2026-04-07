#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <avr/interrupt.h>

static inline void interrupt_init(void) {
    // INT trigger (high to low) on INT2
    EICRA &= ~((1 << ISC20) | (1 << ISC21));
    EICRA |=  (1 << ISC21);
    EIMSK |=  (1 << INT2);
}

static inline void interrupt_enable() { sei(); }
static inline void interrupt_disable() { cli(); }

#endif
