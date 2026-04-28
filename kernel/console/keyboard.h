#ifndef KEYBOARD_IO_H
#define KEYBOARD_IO_H

void kb_init(void);

char kb_getc(void);
void kb_get_raw(uint8_t* low_byte, uint8_t* high_byte);

uint8_t kb_check_input_state(void);
uint8_t kb_get_current_char(void);
void kb_clear_input_state(void);

char kb_getc(void);
void kb_get_raw(uint8_t* low_byte, uint8_t* high_byte);

void kb_isr_callback(void);
void kb_event_handler(void);

#endif
