#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

uint8_t cursor_position;
uint8_t cursor_line;

uint8_t display_width;
uint8_t display_height;

#ifdef KERNEL_PLATFORM_AVR
  #include <kernel/boot/avr/interrupt.h>
  
  #include <kernel/kernel.h>
  #include <kernel/bus/bus.h>
  #include <kernel/arch/avr/io.h>
  
  #include <kernel/console/virtual_key.h>
  #include <kernel/console/keyboard.h>
  #include <kernel/console/console.h>
  #include <kernel/console/print.h>
  
  struct Bus display_bus;
  
  uint32_t display_address;
#endif

#ifdef KERNEL_PLATFORM_X86
  #include <kernel/arch/x86/io.h>
  
  #include <kernel/console/print.h>
  
  #define CRTC_INDEX_PORT 0x3D4
  #define CRTC_DATA_PORT  0x3D5
  
  volatile uint16_t* volatile display_address = (volatile uint16_t*)0xB8000;
  
  static void x86_sync_hardware_cursor(void) {
      // Calculate the flat 1D index: (Row * Width) + Column
      uint16_t flat_position = (cursor_line * display_width) + cursor_position;
      
      // Send the high byte to Register 0x0E
      outb(CRTC_INDEX_PORT, 0x0E);
      outb(CRTC_DATA_PORT,  (uint8_t)((flat_position >> 8) & 0xFF));
      
      // Send the low byte to Register 0x0F
      outb(CRTC_INDEX_PORT, 0x0F);
      outb(CRTC_DATA_PORT,  (uint8_t)(flat_position & 0xFF));
  }
#endif

void display_init(void) {
#ifdef KERNEL_PLATFORM_AVR
    display_bus.write_waitstate = 20;
    display_bus.read_waitstate  = 20;
    
    display_width  = 21;
    display_height = 8;
    
    cursor_line = 0;
    cursor_position = 0;
    
    display_address = device_get_hardware_address("display");
    
    if (display_address == 0) 
        return;
#endif
    
#ifdef KERNEL_PLATFORM_X86
    
    display_width  = 80;
    display_height = 25;
    
    cursor_line = 0;
    cursor_position = 0;
    
#endif
}

uint32_t display_get_device_address(void) {
#ifdef KERNEL_PLATFORM_AVR
    return display_address;
#endif
    return 0;
}

void display_busy_wait(void) {
#ifdef KERNEL_PLATFORM_AVR
    if (display_address == 0) 
        return;
    uint8_t checkByte = 0;
    mmio_readb(&display_bus, display_address, &checkByte);
    while (checkByte != 0x10) {
        mmio_readb(&display_bus, display_address, &checkByte);
    }
#endif
}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_height;
}


void display_putc(const char ch) {
#ifdef KERNEL_PLATFORM_X86
    char charry[2] = {ch, '\0'};
    print(charry);
#endif
    
#ifdef KERNEL_PLATFORM_AVR
    display_busy_wait();
    if (display_address == 0) 
        return;
    mmio_writeb(&display_bus, display_address + 0x00010, (uint8_t*)&ch);
#endif
}

void display_newline(void) {
#ifdef KERNEL_PLATFORM_AVR
    uint8_t one = 1;
    if (display_address == 0) 
        return;
    mmio_writeb(&display_bus, display_address + 0x00005, &one);
    display_busy_wait();
#endif
}

void display_cursor_set_position(uint16_t position) {
#ifdef KERNEL_PLATFORM_X86
    // Guard against bounds, update global state, and sync
    if (position >= display_width) position = display_width - 1;
    cursor_position = (uint8_t)position;
    x86_sync_hardware_cursor();
#endif
    
#ifdef KERNEL_PLATFORM_AVR
    display_busy_wait();
    if (display_address == 0) 
        return;
    if (position >= display_get_width()) position = display_width - 1;
    cursor_position = position;
    mmio_writeb(&display_bus, display_address + 0x00002, &cursor_position);
#endif
}

void display_cursor_set_line(uint16_t line) {
#ifdef KERNEL_PLATFORM_X86
    // Guard against bounds, update global state, and sync
    // (Assuming standard VGA height of 25)
    if (line >= 25) line = 25 - 1; 
    cursor_line = (uint8_t)line;
    x86_sync_hardware_cursor();
#endif
    
#ifdef KERNEL_PLATFORM_AVR
    if (display_address == 0) 
        return;
    display_busy_wait();
    if (line >= display_get_height()) line = display_height - 1;
    cursor_line = line;
    mmio_writeb(&display_bus, display_address + 0x00001, &cursor_line);
#endif
}

uint16_t display_cursor_get_position(void) {
    return cursor_position;
}

uint16_t display_cursor_get_line(void) {
    return cursor_line;
}

void display_clear(void) {
#ifdef KERNEL_PLATFORM_X86
    for (int i = 0; i < display_width * display_height; i++) 
        display_address[i] = (uint16_t)' ' | (uint16_t)(15 | (0 << 4)) << 8;
#endif
    
#ifdef KERNEL_PLATFORM_AVR
    display_busy_wait();
    display_cursor_set_position(0);
    display_cursor_set_line(0);
    
    for (uint8_t h=0; h < display_height; h++) 
        for (uint8_t w=0; w < display_width; w++) 
            print(" ");
    display_cursor_set_position(0);
    display_cursor_set_line(0);
    return;
#endif
}

void console_set_blink_rate(uint8_t rate) {
#ifdef KERNEL_PLATFORM_X86
    // Select the Cursor Start Register (0x0A) on the CRTC
    outb(CRTC_INDEX_PORT, 0x0A);
    
    // Read current register value to preserve scanline settings
    uint8_t reg_val = inb(CRTC_DATA_PORT);
    
    if (rate == 0) {
        reg_val |= 0x20; // Disable cursor
    } else {
        reg_val &= ~0x20; // Enable cursor
    }
    
    outb(CRTC_INDEX_PORT, 0x0A);
    outb(CRTC_DATA_PORT, reg_val);
#endif
    
#ifdef KERNEL_PLATFORM_AVR
    display_busy_wait();
    if (display_address == 0) 
        return;
    mmio_writeb(&display_bus, display_address + 0x00003, &rate);
#endif
}
