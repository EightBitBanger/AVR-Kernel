#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <stdint.h>

typedef uint16_t wEvent;

// Window events

#define EVENT_REDRAW                    0x0001
#define EVENT_MOUSE                     0x0002
#define EVENT_KEYBOARD                  0x0004

#define EVENT_CLOSE                     0x0008
#define EVENT_SHOW                      0x0010
#define EVENT_HIDE                      0x0020
#define EVENT_MINIMIZE                  0x0040
#define EVENT_RESIZE                    0x0080

#define EVENT_DESTROY                   0x0100

// Event status

#define EVENT_STATE_MOUSE_BTN_LEFT      0x00000001
#define EVENT_STATE_MOUSE_BTN_RIGHT     0x00000002
#define EVENT_STATE_MOUSE_DOUBLE_CLK    0x00000004

#endif
