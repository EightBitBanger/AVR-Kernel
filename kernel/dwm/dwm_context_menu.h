#ifndef _CONTEXT_MENU_H_
#define _CONTEXT_MENU_H_

#include <stdint.h>

#define DWM_CONTEXT_MENU_DESKTOP     0x01
#define DWM_CONTEXT_MENU_ICON        0x02
#define DWM_CONTEXT_MENU_USER        0x04

// Mouse button triggers

void dwm_icon_right_click(struct WindowContext* ctx);
void dwm_desktop_right_click(struct WindowContext* ctx);

// Event callbacks

void dwm_desktop_context_callback(struct WindowContext* ctx, uint16_t index);
void dwm_icon_context_callback(struct WindowContext* ctx, uint16_t index);

#endif
