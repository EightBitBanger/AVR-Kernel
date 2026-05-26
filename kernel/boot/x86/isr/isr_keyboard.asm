global isr_keyboard
extern keyboard_handler_c

isr_keyboard:
    pusha          ; Save all general-purpose registers
    
    call keyboard_handler_c
    
    popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
