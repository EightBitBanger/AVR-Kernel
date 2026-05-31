extern isr_callback_div_zero_handler

global isr_div_zero
    
isr_div_zero:
    pusha          ; Save all general-purpose registers
    
    ; Allocate 512 bytes on the stack for FPU/SSE state + 16 bytes for alignment safety
    sub esp, 528           
    mov eax, esp
    add eax, 15
    and eax, 0xFFFFFFF0    ; Aligns EAX to a strict 16-byte boundary
    
    fxsave [eax]           ; Save the entire FPU/MMX/SSE/XMM state
    
    call isr_callback_div_zero_handler
    
    ; Re-calculate the exact same 16-byte aligned address to restore
    mov eax, esp
    add eax, 15
    and eax, 0xFFFFFFF0
    
    fxrstor [eax]          ; Restore the entire FPU/MMX/SSE/XMM state
    
    add esp, 528           ; Clean up the stack allocation
    popa                   ; Restore registers
    iret                   ; Interrupt return (clears flags, restores CS/EIP)
    
