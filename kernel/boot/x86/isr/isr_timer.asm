extern isr_callback_timer_handler

global isr_timer
    
isr_timer:
    pusha          ; Save all general-purpose registers
    
    call isr_callback_timer_handler
    
    popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
    
