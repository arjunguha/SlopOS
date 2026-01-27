BITS 32
GLOBAL context_switch

; void context_switch(unsigned int **old_esp, unsigned int *new_esp)
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]
    mov [eax], esp
    mov esp, [esp + 24]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
