section .multiboot
align 4
    dd 0x1BADB002         ; MAGIC
    dd 0x00000003         ; FLAGS (ALIGNED + MEMINFO)
    dd -(0x1BADB002 + 0x00000003) ; CHECKSUM

section .bss
align 16
stack_bottom:
resb 16384 ; 16 KiB
stack_top:

section .text
global _start
_start:
    mov esp, stack_top       ; Set up stack
    and esp, 0xFFFFFFF0      ; Force 16-byte alignment
    
    extern kmain
    call kmain

.hang:
    hlt                  
    jmp .hang
