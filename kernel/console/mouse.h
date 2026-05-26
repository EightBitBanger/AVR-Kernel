#ifndef MOUSE_H
#define MOUSE_H

#include <stdbool.h>

#define MOUSE_BUTTON_LEFT    0
#define MOUSE_BUTTON_MIDDLE  1
#define MOUSE_BUTTON_RIGHT   2

struct PointStruct {
    int32_t x;
    int32_t y;
};typedef struct PointStruct Point;

struct RectangleStruct {
    int32_t x;
    int32_t y;
};typedef struct RectangleStruct Rect;


void mouse_initiate(void);

void mouse_set_cursor_speed(int32_t horz, int32_t vert);

void mouse_set_cursor_acceleration(int32_t acceleration);

void mouse_event_handler(void);

void mouse_set_position(uint32_t x, uint32_t y);
Point mouse_get_position(void);

bool mouse_get_button(uint8_t button);

#endif
