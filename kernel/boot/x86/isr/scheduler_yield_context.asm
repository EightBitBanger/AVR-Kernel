global scheduler_yield_context
extern thread_handler_c

align 4

scheduler_yield_context:
    pushad
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp
    call thread_handler_c
    add esp, 4
    
    mov esp, eax
    
    pop gs
    pop fs
    pop es
    pop ds
    popad
    iret
    
