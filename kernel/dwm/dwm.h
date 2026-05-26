#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <kernel/arch/x86/drivers/draw.h>
#include <kernel/console/mouse.h>
#include <kernel/console/display.h>

#include <kernel/dwm/icons.h>
#include <kernel/dwm/window_context.h>
#include <kernel/dwm/window_handle.h>
#include <kernel/dwm/rendering/sprite.h>

#define WINDOW_FLAG_REDRAW  0x01

struct WindowHandle* create_window(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void destroy_window(struct WindowHandle* window_handle);

void dwm_initiate(void);
void dwm_update(void);

void dwm_set_focus(struct WindowHandle* target);

#endif
