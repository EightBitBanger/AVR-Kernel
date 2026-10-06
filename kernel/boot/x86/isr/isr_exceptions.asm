; Generic CPU exception stubs (vectors 0-31).
;
; Previously every vector pointed at isr_dummy, which sent a PIC EOI and
; executed iret. For exceptions that push an error code that iret pops the
; error code as EIP (double fault -> triple fault), and for #DE it returns to
; the same faulting instruction forever. These stubs report and halt instead.

extern isr_callback_exception_handler

global isr_exception_table

%macro EXC_NOERR 1
isr_exc%1:
    push dword 0           ; dummy error code
    push dword %1          ; vector
    jmp exception_common
%endmacro

%macro EXC_ERR 1
isr_exc%1:
    push dword %1          ; vector (CPU already pushed the error code)
    jmp exception_common
%endmacro

section .text

EXC_NOERR 0
EXC_NOERR 1
EXC_NOERR 2
EXC_NOERR 3
EXC_NOERR 4
EXC_NOERR 5
EXC_NOERR 6
EXC_NOERR 7
EXC_ERR   8
EXC_NOERR 9
EXC_ERR   10
EXC_ERR   11
EXC_ERR   12
EXC_ERR   13
EXC_ERR   14
EXC_NOERR 15
EXC_NOERR 16
EXC_ERR   17
EXC_NOERR 18
EXC_NOERR 19
EXC_NOERR 20
EXC_ERR   21
EXC_NOERR 22
EXC_NOERR 23
EXC_NOERR 24
EXC_NOERR 25
EXC_NOERR 26
EXC_NOERR 27
EXC_NOERR 28
EXC_ERR   29
EXC_ERR   30
EXC_NOERR 31

; Stack on entry: [vector] [error code] [EIP] [CS] [EFLAGS]
exception_common:
    cli
    mov eax, [esp + 8]     ; EIP
    mov ebx, [esp + 4]     ; error code
    mov ecx, [esp + 0]     ; vector
    
    ; Align ESP for the C call (i386 SysV ABI): 4 (pad) + 3 arguments * 4 = 16
    and esp, 0xFFFFFFF0
    sub esp, 4
    
    ; cdecl: isr_callback_exception_handler(vector, error_code, eip)
    push eax
    push ebx
    push ecx
    call isr_callback_exception_handler
    
.hang:
    cli
    hlt
    jmp .hang

section .rodata
align 4
isr_exception_table:
%assign i 0
%rep 32
    dd isr_exc %+ i
%assign i i+1
%endrep
