#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdbool.h>

#include <kernel/dwm/rendering/image.h>

#include <kernel/dwm/icons.h>
#include <kernel/dwm/icon_object.h>

#include <kernel/dwm/window_context.h>
#include <kernel/dwm/window_object.h>
#include <kernel/dwm/window_handle.h>
#include <kernel/dwm/rendering/sprite.h>
#include <kernel/dwm/context_menu.h>

#include <kernel/dwm/flags.h>
#include <kernel/dwm/style_flags.h>

typedef void(*WindowProcedure)(WindowHandle, wEvent);

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} WindowClass;

void dwm_initiate(void);
void dwm_update(void);

WindowHandle create_window(WindowClass w_class, uint16_t w_style, WindowProcedure proc);
void destroy_window(WindowHandle window_handle);

void create_folder(uint16_t x, uint16_t y, const char* name);

void dwm_window_set_parent(WindowHandle child, WindowHandle parent);
WindowHandle dwm_window_get_parent(WindowHandle window);

struct IconObject* create_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t* sprite);
void destroy_icon(struct IconObject* icon);

// Primitive drawing

void dwm_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect_filled(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);

void dwm_draw_text(int16_t x, int16_t y, const char* text, uint32_t color);
void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h);
void dwm_draw_sprite(int16_t x, int16_t y, struct Image* sprite_image);

void dwm_set_cursor(uint32_t* sprite, int16_t width, int16_t height);

void dwm_window_set_focus(WindowHandle handle);

void dwm_window_send_event(WindowHandle handle, wEvent event);

uint32_t dwm_window_get_count(void);

#endif
