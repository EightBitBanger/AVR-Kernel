#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <stdint.h>

typedef uint16_t wEvent;

#define EVENT_REDRAW            0x0001
#define EVENT_MOUSE             0x0002
#define EVENT_KEYBOARD          0x0004

#define EVENT_CLOSE             0x0008
#define EVENT_SHOW              0x0010
#define EVENT_HIDE              0x0020

#endif
