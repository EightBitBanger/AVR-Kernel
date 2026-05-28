extern isr_callback_div_zero_handler

global isr_div_zero
    
isr_div_zero:
    pusha          ; Save all general-purpose registers
    
    call isr_callback_div_zero_handler
    
    popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
    
