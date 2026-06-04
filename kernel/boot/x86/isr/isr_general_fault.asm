extern isr_callback_fault_handler

global isr_general_fault

isr_general_fault:
    
    ; The CPU has already pushed EFLAGS, CS, EIP, and the Error Code.
    ; Stack currently looks like: [EFLAGS] -> [CS] -> [EIP] -> [Error Code] <- ESP
    
    ; Push all general-purpose registers to preserve state for the panic screen
    pusha 
    
    ; Allocate 512 bytes on the stack for FPU/SSE state + 16 bytes for alignment safety
    sub esp, 528           
    mov eax, esp
    add eax, 15
    and eax, 0xFFFFFFF0    ; Aligns EAX to a strict 16-byte boundary
    
    fxsave [eax]           ; Save the entire FPU/MMX/SSE/XMM state
    
    push 0x00    ; Third argument to C function: fault type
    
    ; Read the faulting address from CR2
    mov eax, cr2
    push eax    ; Second argument to C function: faulting_address
    
    ; Get the error code (its now shifted down because of pusha + push eax)
    ; pusha takes 32 bytes + 4 bytes for eax = 36 bytes. 
    ; The error code is at esp + 36
    mov ebx, [esp + 36] 
    push ebx    ; First argument to C function: error_code
    
    call isr_callback_fault_handler
    
    cli
    hlt
    
    
