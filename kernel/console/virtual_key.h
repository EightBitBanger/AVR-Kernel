#ifndef VIRTUAL_KEY_H
#define VIRTUAL_KEY_H

#define VK_ALT            0x11
#define VK_CONTROL        0x14
#define VK_DELETE         0x71
#define VK_SHIFT_LEFT     0x12
#define VK_SHIFT_RIGHT    0x59

#define VK_INSERT         0x70
#define VK_HOME           0x6C
#define VK_END            0x69
#define VK_PAGE_UP        0x7D
#define VK_PAGE_DOWN      0x7A

#define VK_CAPSLOCK       0x58

#define VK_ENTER          0x5A
#define VK_SPACE          0x29
#define VK_BACKSPACE      0x66
#define VK_TAB            0x0D

#define VK_UP             0x75
#define VK_DOWN           0x72
#define VK_LEFT           0x6B
#define VK_RIGHT          0x74

#define VK_ESCAPE         0x76
#define VK_F1             0x05
#define VK_F2             0x06
#define VK_F3             0x04
#define VK_F4             0x0C
#define VK_F5             0x03
#define VK_F6             0x0B
#define VK_F7             0x83
#define VK_F8             0x0A
#define VK_F9             0x01
#define VK_F10            0x09
#define VK_F11            0x78
#define VK_F12            0x07

#define VK_A              0x1C
#define VK_B              0x32
#define VK_C              0x21
#define VK_D              0x23
#define VK_E              0x24
#define VK_F              0x2B
#define VK_G              0x34
#define VK_H              0x33
#define VK_I              0x43
#define VK_J              0x3B
#define VK_K              0x42
#define VK_L              0x4B
#define VK_M              0x3A
#define VK_N              0x31
#define VK_O              0x44
#define VK_P              0x4D
#define VK_Q              0x15
#define VK_R              0x2D
#define VK_S              0x1B
#define VK_T              0x2C
#define VK_U              0x3C
#define VK_V              0x2A
#define VK_W              0x1D
#define VK_X              0x22
#define VK_Y              0x35
#define VK_Z              0x1A

#define VK_0              0x45
#define VK_1              0x16
#define VK_2              0x1E
#define VK_3              0x26
#define VK_4              0x25
#define VK_5              0x2E
#define VK_6              0x36
#define VK_7              0x3D
#define VK_8              0x3E
#define VK_9              0x46

#define VK_SEMICOLON      0x4C
#define VK_EQUALS         0x55
#define VK_COMMA          0x41
#define VK_MINUS          0x4E
#define VK_PERIOD         0x49
#define VK_SLASH          0x4A
#define VK_BACKTICK       0x0E
#define VK_BRACKET_LEFT   0x54
#define VK_BACKSLASH      0x5D
#define VK_BRACKET_RIGHT  0x5B
#define VK_QUOTE          0x52

#include <stdint.h>

void kb_map_init(uint8_t* map_ptr, uint16_t size);

char kb_vkey_check(uint8_t key);
char kb_vkey_set(uint8_t key, uint8_t state);

#endif
