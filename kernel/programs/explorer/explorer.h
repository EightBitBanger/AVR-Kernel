#ifndef PROGRAM_EXPLORER_H
#define PROGRAM_EXPLORER_H

#include <kernel/events.h>

#define MAX_ITEMS      64
#define MAX_TITLE_LEN  16
#define MAX_PATH_LEN   256

#define TILE_BASE_X         5
#define TILE_BASE_Y        40
#define TILE_WIDTH         72
#define TILE_HEIGHT        80

#define ICON_FOLDER         0
#define ICON_FILE           1
#define ICON_DOCUMENT       2
#define ICON_SYSTEM         3
#define ICON_STORAGE        4

#define MAX_PATH_DEPTH      4

void explorer_main(const char* arguments);

#endif
