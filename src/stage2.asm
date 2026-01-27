; Stage 2 loader: loads kernel, collects E820 memory map, enters 32-bit mode,
; provides a basic console (serial + VGA), and a bump allocator stub.

BITS 16
ORG 0x8000

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 4
%endif

%ifndef KERNEL_LBA
%define KERNEL_LBA 5
%endif

KERNEL_LOAD_SEG  equ 0x1000
KERNEL_LOAD_OFF  equ 0x0000
KERNEL_LOAD_ADDR equ 0x10000
KERNEL_ENTRY     equ 0x00100000

SECTORS_PER_TRACK equ 18
HEADS_PER_CYL     equ 2

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    call enable_a20
    ; Keep real-mode output minimal; C kernel prints the banner.
    call load_kernel
    call e820_probe

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEL:pmode_entry

; ----- Real mode helpers -----

print_string_rm:
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

; Fast A20 enable via port 0x92.
enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

; Initialize COM1 serial port for stdio output in QEMU.
serial_init:
    mov dx, 0x3F8 + 1
    mov al, 0x00
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8 + 0
    mov al, 0x03
    out dx, al
    mov dx, 0x3F8 + 1
    mov al, 0x00
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x03
    out dx, al
    mov dx, 0x3F8 + 2
    mov al, 0xC7
    out dx, al
    mov dx, 0x3F8 + 4
    mov al, 0x0B
    out dx, al
    ret

; Load the kernel from disk into low memory at 0x10000.
load_kernel:
    pusha
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov cx, KERNEL_SECTORS
    mov si, KERNEL_LBA

.load_loop:
    push cx
    push bx
    push si
    call read_sector_lba
    pop si
    pop bx
    pop cx

    add bx, 512
    jnc .next
    add ax, 0x20
    mov es, ax
.next:
    inc si
    loop .load_loop

    popa
    ret

; Read one sector from LBA in SI into ES:BX.
read_sector_lba:
    pusha
    mov di, bx

    mov ax, si
    xor dx, dx
    mov bx, SECTORS_PER_TRACK
    div bx
    mov cl, dl
    inc cl

    xor dx, dx
    mov bx, HEADS_PER_CYL
    div bx
    mov ch, al
    mov dh, dl

    mov ah, 0x02
    mov al, 0x01
    mov dl, [boot_drive]
    mov bx, di
    int 0x13
    jc disk_error

    popa
    ret

disk_error:
    mov si, disk_error_msg
    call print_string_rm
.hang:
    hlt
    jmp .hang

; BIOS E820 memory map probe.
; Stores entries at e820_entries and count at e820_count.
; Each entry is 20 bytes.
e820_probe:
    pusha
    xor ax, ax
    mov es, ax
    xor ebx, ebx
    mov di, e820_entries
    xor cx, cx
.next:
    xor ax, ax
    mov es, ax
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 20
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    add di, 20
    inc cx
    test ebx, ebx
    jnz .next
.done:
    mov [e820_count], cx
    popa
    ret

; ----- Protected mode -----

BITS 32
pmode_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FC00

    call copy_kernel
    call KERNEL_ENTRY

.hang:
    hlt
    jmp .hang

; ----- Console (serial + VGA) -----

console_init:
    mov dword [vga_cursor], VGA_BASE
    ret

console_write:
    pusha
.next:
    lodsb
    test al, al
    jz .done
    cmp al, 0x0A
    je .newline
    call console_putc
    jmp .next
.newline:
    call console_newline
    jmp .next
.done:
    popa
    ret

console_newline:
    push eax
    mov al, 0x0D
    call serial_write
    mov al, 0x0A
    call serial_write
    pop eax

    push eax
    push edx
    mov eax, [vga_cursor]
    sub eax, VGA_BASE
    mov ecx, 160
    xor edx, edx
    div ecx
    inc eax
    imul eax, 160
    add eax, VGA_BASE
    mov [vga_cursor], eax
    pop edx
    pop eax
    ret

console_putc:
    push eax
    call serial_write
    pop eax

    push eax
    push edx
    mov edx, [vga_cursor]
    mov ah, 0x07
    mov [edx], ax
    add edx, 2
    mov [vga_cursor], edx
    pop edx
    pop eax
    ret

serial_write:
    push edx
    push ebx
    mov bl, al
.wait:
    mov dx, 0x3F8 + 5
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, 0x3F8
    mov al, bl
    out dx, al
    pop ebx
    pop edx
    ret

console_write_hex:
    pusha
    mov ecx, 8
.hex_loop:
    rol eax, 4
    mov bl, al
    and bl, 0x0F
    cmp bl, 9
    jbe .digit
    add bl, 7
.digit:
    add bl, '0'
    mov al, bl
    call console_putc
    loop .hex_loop
    popa
    ret

; ----- Bump allocator stub -----

heap_init:
    mov dword [heap_base], 0
    mov dword [heap_end], 0
    mov dword [heap_curr], 0

    movzx ecx, word [e820_count]
    mov esi, e820_entries
.find:
    test ecx, ecx
    jz .done

    mov eax, [esi + 16]
    cmp eax, 1
    jne .next

    mov eax, [esi + 0]
    mov edx, [esi + 4]
    test edx, edx
    jnz .next

    mov ebx, [esi + 8]
    mov edx, [esi + 12]
    test edx, edx
    jnz .next

    mov ecx, eax
    add ecx, 15
    and ecx, 0xFFFFFFF0
    cmp ecx, 0x00100000
    jb .next

    add ebx, eax
    cmp ecx, ebx
    jae .next

    mov [heap_base], ecx
    mov [heap_end], ebx
    mov [heap_curr], ecx
    jmp .done

.next:
    add esi, 20
    dec ecx
    jmp .find

.done:
    ret

heap_alloc:
    ; EAX = size, returns pointer in EAX (0 if no heap)
    test eax, eax
    jz .fail
    add eax, 15
    and eax, 0xFFFFFFF0

    mov ebx, [heap_curr]
    test ebx, ebx
    jz .fail

    mov edx, ebx
    add edx, eax
    cmp edx, [heap_end]
    ja .fail

    mov [heap_curr], edx
    mov eax, ebx
    ret

.fail:
    xor eax, eax
    ret

VGA_BASE equ 0xB8000

vga_cursor:
    dd VGA_BASE

heap_base:
    dd 0

heap_end:
    dd 0

heap_curr:
    dd 0

boot_drive:
    db 0

disk_error_msg:
    db 0x0D, 0x0A, "Disk read error.", 0x0D, 0x0A, 0

pm_banner:
    db 0x0A, "SLOPOS 32-BIT MODE", 0x0A, 0

align 8

e820_entries:
    times 20*32 db 0

e820_count:
    dw 0

; ----- GDT -----

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEL equ 0x08
DATA_SEL equ 0x10
copy_kernel:
    mov esi, KERNEL_LOAD_ADDR
    mov edi, KERNEL_ENTRY
    mov ecx, (KERNEL_SECTORS * 512) / 4
    cld
    rep movsd
    ret
