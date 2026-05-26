global isr_dummy
    
isr_dummy:
    ;pusha          ; Save all general-purpose registers
    
    ;call _handler
    
    ;popa           ; Restore registers
    iret           ; Interrupt return (clears flags, restores CS/EIP)
    
