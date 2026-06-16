#ifndef PROGRAM_EXPLORER_H
#define PROGRAM_EXPLORER_H

#include <kernel/events.h>

#define MAX_ITEMS      64
#define MAX_TITLE_LEN  16
#define MAX_PATH_LEN   256

#define ICON_FOLDER         0
#define ICON_FILE           1
#define ICON_DOCUMENT       2
#define ICON_SYSTEM         3
#define ICON_STORAGE        4

#define MAX_PATH_DEPTH      4


#define BUTTON_BACK_X       6
#define BUTTON_BACK_Y       5

#define NAVBAR_X            5
#define NAVBAR_Y           40
#define NAVBAR_WIDTH       72
#define NAVBAR_HEIGHT      80

// Background ---
#define EXPLORER_BG_X               0
#define EXPLORER_BG_Y               0

// Back Button / Left Segment Area ---
#define BACK_BTN_CONTAINER_X        5
#define BACK_BTN_CONTAINER_Y        (BUTTON_BACK_Y + 4)
#define BACK_BTN_CONTAINER_H        16

#define BACK_BTN_BORDER_X           4
#define BACK_BTN_BORDER_Y           (BUTTON_BACK_Y + 3)
#define BACK_BTN_BORDER_H           18

#define BACK_BTN_SPRITE_X           7
#define BACK_BTN_SPRITE_Y           (BUTTON_BACK_Y + 1)
#define BACK_BTN_PADDING_RIGHT      12

// Macro components for internal calculations
#define BACK_BTN_CONTAINER_W(pt_x)  ((pt_x) - 15)
#define BACK_BTN_BORDER_W(pt_x)     ((pt_x) - 14)

// Path Text Input Field (Right Segment)
#define PATH_FIELD_BG_X(pt_x)       ((pt_x) - 5)
#define PATH_FIELD_BG_Y             (BUTTON_BACK_Y + 4)
#define PATH_FIELD_BG_H             16

#define PATH_FIELD_BORDER_X(pt_x)   ((pt_x) - 4)
#define PATH_FIELD_BORDER_Y         (BUTTON_BACK_Y + 3)
#define PATH_FIELD_BORDER_H         18

// Macro components to calculate remaining window width
#define PATH_FIELD_W(pt_x, win_w)   ((win_w) - (pt_x))

// Path Text Typography
#define PATH_TEXT_Y                 (BUTTON_BACK_Y + 8)
#define PATH_FONT_CHAR_WIDTH        6

// Navigation Divider Line
#define NAV_DIVIDER_X               0
#define NAV_DIVIDER_Y               (BUTTON_BACK_Y + 25)
#define NAV_DIVIDER_H               0 

// Grid Item Typography & Elements
#define ITEM_TEXT_Y_OFF             42    
#define ITEM_FONT_CHAR_WIDTH        6

void explorer_main(const char* arguments);

#endif
