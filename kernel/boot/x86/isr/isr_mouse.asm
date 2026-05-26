global isr_mouse
extern mouse_handler_c

isr_mouse:
    pusha          ; Save all general-purpose registers

    call mouse_handler_c

    popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
