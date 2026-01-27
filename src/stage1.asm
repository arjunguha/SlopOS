; Stage 1 boot sector: loads stage 2 from disk and jumps to it.

BITS 16
ORG 0x7C00

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 4
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov bx, STAGE2_LOAD_OFF
    call read_stage2

    jmp 0x0000:STAGE2_LOAD_OFF

; Read STAGE2_SECTORS from LBA 1 into 0x0000:STAGE2_LOAD_OFF.
read_stage2:
    pusha

    mov ah, 0x02
    mov al, [stage2_count]
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    mov bx, STAGE2_LOAD_OFF
    int 0x13
    jc disk_error

    popa
    ret

disk_error:
    mov si, disk_error_msg
    call print_string
.hang:
    hlt
    jmp .hang

; Prints a null-terminated string at DS:SI using BIOS teletype.
print_string:
    pusha
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp .next
.done:
    popa
    ret

boot_drive:
    db 0

stage2_count:
    db STAGE2_SECTORS

STAGE2_LOAD_OFF equ 0x8000

msg_boot:
    db 0x0D, 0x0A, "SLOPOS STAGE1", 0x0D, 0x0A, 0

disk_error_msg:
    db 0x0D, 0x0A, "Disk read error.", 0x0D, 0x0A, 0

TIMES 510-($-$$) DB 0
DW 0xAA55
