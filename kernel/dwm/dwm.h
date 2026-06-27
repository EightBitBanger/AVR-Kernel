#ifndef _WINDOW_MANAGER_H_
#define _WINDOW_MANAGER_H_

#include <stdbool.h>

#include <kernel/dwm/rendering/image.h>

#include <kernel/dwm/images/icon.h>
#include <kernel/dwm/images/ui.h>
#include <kernel/dwm/images/cursor.h>

#include <kernel/dwm/objects/icon_object.h>

#include <kernel/dwm/objects/window_context.h>
#include <kernel/dwm/objects/window_object.h>
#include <kernel/dwm/objects/window_edit.h>
#include <kernel/dwm/objects/window_handle.h>
#include <kernel/dwm/objects/window_class.h>

#include <kernel/dwm/objects/context_menu.h>
#include <kernel/dwm/rendering/sprite.h>

#include <kernel/dwm/style_flags.h>
#include <kernel/dwm/configuration.h>

typedef void(*WindowProcedure)(WindowHandle, wEvent, uint32_t wparam, int32_t lparam);

void dwm_initiate(void);
void dwm_update(void);

void dwm_set_keyboard_char(uint16_t ch);

// Windows

WindowHandle dwm_create_window(WindowClass wclass, uint16_t wstyle, WindowProcedure wproc);
void dwm_destroy_window(WindowHandle window_handle);

// Icons

int8_t dwm_create_folder(uint16_t x, uint16_t y, const char* name, const char* path);
int8_t dwm_create_file(uint16_t x, uint16_t y, const char* name, const char* path);
int8_t dwm_create_mount(uint16_t x, uint16_t y, const char* name, const char* path);

struct IconObject* dwm_create_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, struct Image* sprite, uint16_t icon_index);
void dwm_destroy_icon(struct IconObject* icon);

// Builtin context functions

void dwm_summon_context_menu(WindowHandle window, uint16_t x, uint16_t y, const char** options, uint16_t number_of_items);

WindowHandle dwm_summon_dialog_delete(const char* title, const char* file_path);
WindowHandle dwm_summon_message_box(const char* title, const char* message);
WindowHandle dwm_summon_properties(const char* title, const char* name, const char* file_path, uint16_t icon_index);

// Resource management

void* dwm_resource_find(const char* name);

// Window resource management

uint8_t dwm_window_resource_add(WindowHandle handle, const char* name, void* resource);
uint8_t dwm_window_resource_remove(WindowHandle handle, void* resource);
uint32_t dwm_window_resource_get_count(WindowHandle handle);
void* dwm_window_resource_get_by_index(WindowHandle handle, uint32_t index);
void* dwm_window_resource_get_by_name(WindowHandle handle, const char* name);
void dwm_window_resource_free_all(WindowHandle handle);

// Primitive drawing

void dwm_draw_line(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect_filled(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void dwm_draw_rect_filled_gradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color_low, uint32_t color_high);

void dwm_draw_text(int16_t x, int16_t y, const char* text, uint32_t color);
void dwm_draw_redraw(int16_t x, int16_t y, int16_t w, int16_t h);
void dwm_draw_sprite(int16_t x, int16_t y, struct Image* image);

// Window objects
EditFieldHandle dwm_window_add_edit_field(WindowHandle handle, uint16_t x, uint16_t y, uint16_t width);

bool dwm_window_edit_text(WindowHandle handle, EditFieldHandle edit_field_handle, const char* text);
bool dwm_window_edit_visible(WindowHandle handle, EditFieldHandle edit_field_handle, bool enable);
bool dwm_window_edit_insert(WindowHandle handle, EditFieldHandle edit_field_handle, const char* text);
bool dwm_window_edit_backspace(WindowHandle handle, EditFieldHandle edit_field_handle);
bool dwm_window_edit_cursor_set_pos(WindowHandle handle, EditFieldHandle edit_field_handle, uint16_t position);
bool dwm_window_edit_set_pos(WindowHandle handle, EditFieldHandle edit_field_handle, uint16_t x, uint16_t y);
bool dwm_window_edit_set_width(WindowHandle handle, EditFieldHandle edit_field_handle, uint16_t width);
size_t dwm_window_edit_get_len(WindowHandle handle, EditFieldHandle edit_field_handle);
bool dwm_window_edit_get_text(WindowHandle handle, EditFieldHandle edit_field_handle, char* name_buffer, size_t buffer_size);

// Window settings

void dwm_window_set_parent(WindowHandle child, WindowHandle parent);
WindowHandle dwm_window_get_parent(WindowHandle window);

uint16_t dwm_window_get_width(WindowHandle handle);
uint16_t dwm_window_get_height(WindowHandle handle);

uint8_t dwm_window_set_name(WindowHandle handle, const char* name);

void dwm_set_cursor(uint32_t* sprite, int16_t width, int16_t height);

uint16_t dwm_get_titlebar_height(WindowHandle handle);

void dwm_window_set_focus(WindowHandle handle);

// Send an event message to the currently focused window
void dwm_send_event(wEvent event);

// Send an even to a specific window
void dwm_window_send_event(WindowHandle handle, wEvent event);

uint32_t dwm_window_get_count(void);

#endif
