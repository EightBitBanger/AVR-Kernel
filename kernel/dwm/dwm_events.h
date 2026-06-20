#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include <stdint.h>

typedef uint16_t wEvent;

// Window events

#define DWM_EVENT_REDRAW                    0x0001
#define DWM_EVENT_MOUSE                     0x0002
#define DWM_EVENT_KEYBOARD                  0x0004

#define DWM_EVENT_CLOSE                     0x0008
#define DWM_EVENT_SHOW                      0x0010
#define DWM_EVENT_HIDE                      0x0020
#define DWM_EVENT_MINIMIZE                  0x0040
#define DWM_EVENT_RESIZE                    0x0080

#define DWM_EVENT_DESTROY                   0x0100

// Event status

#define DWM_STATE_MOUSE_BTN_LEFT      0x00000001
#define DWM_STATE_MOUSE_BTN_RIGHT     0x00000002
#define DWM_STATE_MOUSE_DOUBLE_CLK    0x00000004

#endif
