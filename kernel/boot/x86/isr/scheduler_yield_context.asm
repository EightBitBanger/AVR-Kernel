global scheduler_yield_context
extern thread_handler_c

align 4

; int $0x80: voluntary reschedule (thread_yield). Same frame layout as
; scheduler_context, without the timer tick or EOI.
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
    
    mov ebx, esp               ; Frame pointer
    and esp, 0xFFFFFFF0        ; ESP % 16 == 0 at the CALL
    
    sub esp, 12                ; 12 + 4 (argument) = 16
    push ebx                   ; Argument: address of the saved frame
    call thread_handler_c
    
    mov esp, eax               ; Switch to the chosen thread's saved frame
    
    pop gs
    pop fs
    pop es
    pop ds
    popad
    iret
