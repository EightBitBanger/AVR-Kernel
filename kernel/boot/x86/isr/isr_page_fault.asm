extern isr_callback_fault_handler

global isr_page_fault

isr_page_fault:
    
    ; The CPU has already pushed EFLAGS, CS, EIP, and the Error Code.
    ; Stack currently looks like: [EFLAGS] -> [CS] -> [EIP] -> [Error Code] <- ESP
    
    ; Push all general-purpose registers to preserve state for the panic screen
    pusha                  ; error code now at esp + 32
    
    mov ebx, [esp + 32]    ; Error code
    mov ecx, cr2           ; Faulting linear address
    
    ; The handler never returns, so no FPU state needs saving. Align ESP for
    ; the C call: 4 (pad) + 3 arguments * 4 = 16.
    and esp, 0xFFFFFFF0
    sub esp, 4
    
    push dword 0x01        ; Third argument: fault type (PT_PAGE_FAULT)
    push ecx               ; Second argument: faulting_address
    push ebx               ; First argument: error_code
    
    call isr_callback_fault_handler
    
    ; The handler never returns
    cli
.hang:
    hlt
    jmp .hang
