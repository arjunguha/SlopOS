BITS 32
GLOBAL _start
EXTERN kmain

_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FC00
    call kmain

.hang:
    hlt
    jmp .hang
