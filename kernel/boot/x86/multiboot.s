section .multiboot
align 4
    dd 0x1BADB002                 ; MAGIC
    dd 0x00000003                 ; FLAGS (ALIGNED + MEMINFO)
    dd -(0x1BADB002 + 0x00000003) ; CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                    ; 16 KiB
stack_top:

section .text
global _start
_start:
    ; 1. Set up the stack safely
    mov esp, stack_top      
    and esp, 0xFFFFFFF0     
    
    ; -------------------------------------------------------------------------
    ; FLOATING POINT & SSE INITIALIZATION
    ; -------------------------------------------------------------------------
    
    ; Step A: Configure CR0 for the standard FPU
    mov eax, cr0
    and eax, ~(1 << 2)            ; Clear EM (Bit 2) - Disable FPU emulation
    or eax, (1 << 1) | (1 << 5)   ; Set MP (Bit 1) & NE (Bit 5) - Monitor Coprocessor & Numeric Error
    mov cr0, eax
    finit                         ; Initialize FPU state

    ; Step B: Check for SSE support via CPUID before touching CR4
    mov eax, 0x1
    cpuid
    test edx, (1 << 25)           ; Bit 25 of EDX indicates SSE support
    jz .skip_sse                  ; If not supported, skip SSE activation to avoid crashing

    ; Step C: Configure CR4 for SSE
    mov eax, cr4
    or eax, (1 << 9) | (1 << 10)  ; Set OSFXSR (Bit 9) & OSXMMEXCPT (Bit 10)
    mov cr4, eax

.skip_sse:

    ; -------------------------------------------------------------------------
    ; VGA MODE 13h SETUP (320x200, 256 Colors)
    ; -------------------------------------------------------------------------

    ; 1. Reset Sequencer
    mov dx, 0x3C4
    mov al, 0x00
    out dx, al
    mov dx, 0x3C5
    mov al, 0x01   ; Synchronous Reset
    out dx, al

    ; 2. Miscellaneous Output: Clock & Architecture setup
    mov dx, 0x3C2
    mov al, 0x63   ; Select 25.175 MHz clock, enable CPU access
    out dx, al

    ; 3. Sequencer: Enable Chain-4 and allow writing to all 4 planes
    mov dx, 0x3C4
    mov al, 0x02   ; Map Mask Register
    out dx, al
    mov dx, 0x3C5
    mov al, 0x0F   ; Enable all 4 planes for writing
    out dx, al

    mov dx, 0x3C4
    mov al, 0x04   ; Memory Mode Register
    out dx, al
    mov dx, 0x3C5
    mov al, 0x0E   ; Enable Chain-4 mode (creates flat memory)
    out dx, al

    ; 4. CRT Controller: Un-protect registers 0-7
    mov dx, 0x3D4
    mov al, 0x11   ; End Vertical Blank register
    out dx, al
    mov dx, 0x3D5
    in al, dx
    and al, 0x7F   ; Clear bit 7 to remove write-protection
    out dx, al

    ; 5. CRT Controller: Exact Standard 320x200 Timings
    ; --- Horizontal Timings ---
    mov dx, 0x3D4
    mov al, 0x00   ; Horizontal Total
    out dx, al
    mov dx, 0x3D5
    mov al, 0x5F
    out dx, al

    mov dx, 0x3D4
    mov al, 0x01   ; Horizontal Display Enable End
    out dx, al
    mov dx, 0x3D5
    mov al, 0x4F
    out dx, al

    ; --- Vertical Timings & Scan Lines ---
    mov dx, 0x3D4
    mov al, 0x09   ; Maximum Scan Line Register
    out dx, al
    mov dx, 0x3D5
    mov al, 0x41   ; Bit 6 = 1 (Enable Double Scan), Bits 0-4 = 1 (200->400 lines)
    out dx, al

    mov dx, 0x3D4
    mov al, 0x12   ; Vertical Display Enable End
    out dx, al
    mov dx, 0x3D5
    mov al, 0x8F   ; Tell the beam exactly where the 200-line boundary ends
    out dx, al

    mov dx, 0x3D4
    mov al, 0x14   ; Underline Location
    out dx, al
    mov dx, 0x3D5
    mov al, 0x40   ; Double-word mode (Required for Chain-4)
    out dx, al

    mov dx, 0x3D4
    mov al, 0x17   ; CRTC Mode Control
    out dx, al
    mov dx, 0x3D5
    mov al, 0xA3   ; Enable byte-mode addressing
    out dx, al
    
    ; 6. Graphics Controller: Configure Interleave 
    mov dx, 0x3CE
    mov al, 0x05   ; Graphics Mode Register
    out dx, al
    mov dx, 0x3CF
    mov al, 0x40   ; Enable 256-color shift mode
    out dx, al

    mov dx, 0x3CE
    mov al, 0x06   ; Miscellaneous Register
    out dx, al
    mov dx, 0x3CF
    mov al, 0x05   ; Map VGA memory starting at 0xA0000 (64KB window)
    out dx, al

    ; 7. Wake the Sequencer back up
    mov dx, 0x3C4
    mov al, 0x00
    out dx, al
    mov dx, 0x3C5
    mov al, 0x03   ; Clear Reset
    out dx, al
    
    ; 3. --- JUMP TO C KERNEL ---
    extern kmain
    call kmain

.hang:
    hlt                
    jmp .hang
