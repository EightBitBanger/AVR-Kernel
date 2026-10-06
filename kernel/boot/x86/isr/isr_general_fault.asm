extern isr_callback_fault_handler

global isr_general_fault

isr_general_fault:
    
    ; The CPU has already pushed EFLAGS, CS, EIP, and the Error Code.
    ; Stack currently looks like: [EFLAGS] -> [CS] -> [EIP] -> [Error Code] <- ESP
    ; For #GP the error code is a segment selector index (or 0), not page-fault bits.
    
    pusha                  ; error code at esp + 32, EIP at esp + 36
    
    mov ebx, [esp + 32]    ; Error code (selector)
    mov ecx, [esp + 36]    ; Faulting EIP (CR2 is meaningless for #GP)
    
    ; The handler never returns. Align ESP for the C call: 4 + 3 * 4 = 16.
    and esp, 0xFFFFFFF0
    sub esp, 4
    
    push dword 0x00        ; Third argument: fault type (PT_GENERAL_PROTECTION_FAULT)
    push ecx               ; Second argument: faulting EIP
    push ebx               ; First argument: error_code
    
    call isr_callback_fault_handler
    
    cli
.hang:
    hlt
    jmp .hang
