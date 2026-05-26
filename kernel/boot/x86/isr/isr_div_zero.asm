extern c_interrupt_handler

global isr_div_zero
    
isr_div_zero:
    pusha          ; Save all general-purpose registers
    
    call c_interrupt_handler
    
    popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
    
