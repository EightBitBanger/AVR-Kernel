extern isr_callback_page_fault_handler

global isr_page_fault

isr_page_fault:
    
    ; The CPU has already pushed EFLAGS, CS, EIP, and the Error Code.
    ; Stack currently looks like: [EFLAGS] -> [CS] -> [EIP] -> [Error Code] <- ESP
    
    ; Push all general-purpose registers to preserve state for the panic screen
    pusha 
    
    ; Read the faulting address from CR2
    mov eax, cr2
    push eax    ; Second argument to C function: faulting_address
    
    ; Get the error code (its now shifted down because of pusha + push eax)
    ; pusha takes 32 bytes + 4 bytes for eax = 36 bytes. 
    ; The error code is at esp + 36
    mov ebx, [esp + 36] 
    push ebx    ; First argument to C function: error_code
    
    ; 5. Call your fancy C panic handler
    call isr_callback_page_fault_handler
    
    ; (Optional) If your handler ever returned, you'd clean up the stack here,
    ; but since it loops infinitely, execution stops here.
    cli
    hlt
    
    
