#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdbool.h>

#include <kernel/dwm/dwm_platform.h>

#include <kernel/dwm/rendering/image.h>

#include <kernel/dwm/images/icon.h>
#include <kernel/dwm/images/ui.h>
#include <kernel/dwm/images/cursor.h>

#include <kernel/dwm/objects/icon_object.h>

#include <kernel/dwm/objects/window_context.h>
#include <kernel/dwm/objects/window_object.h>
#include <kernel/dwm/objects/window_handle.h>
#include <kernel/dwm/objects/context_menu.h>
#include <kernel/dwm/rendering/sprite.h>

#include <kernel/dwm/flags.h>
#include <kernel/dwm/style_flags.h>

#define DOUBLE_CLICK_THRESHOLD_MS   500

typedef void(*WindowProcedure)(WindowHandle, wEvent, uint16_t wparam);

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t max_width;
    uint16_t max_height;
    char* title;
} WindowClass;


// Testing - non user functions

void* dwm_resource_find(const char* name);


void dwm_initiate(void);
void dwm_update(void);

WindowHandle create_window(WindowClass wclass, uint16_t wstyle, WindowProcedure wproc);
void destroy_window(WindowHandle window_handle);

int8_t create_folder(uint16_t x, uint16_t y, const char* name);
int8_t create_file(uint16_t x, uint16_t y, const char* name);

void dwm_window_set_parent(WindowHandle child, WindowHandle parent);
WindowHandle dwm_window_get_parent(WindowHandle window);

struct IconObject* create_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, struct Image* sprite);
void destroy_icon(struct IconObject* icon);

// Primitive drawing

void dwm_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect_filled(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect_filled_gradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color_low, uint32_t color_high);

void dwm_draw_text(int16_t x, int16_t y, const char* text, uint32_t color);
void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h);
void dwm_draw_sprite(int16_t x, int16_t y, struct Image* sprite_image);

void dwm_set_cursor(uint32_t* sprite, int16_t width, int16_t height);

void dwm_window_set_focus(WindowHandle handle);

void dwm_window_send_event(WindowHandle handle, wEvent event);

uint32_t dwm_window_get_count(void);

#endif
