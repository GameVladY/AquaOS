; =====================================================================
;  AquaOS boot loader  (NASM, BIOS / El Torito "no emulation" boot)
;
;  The BIOS loads this whole image to 0x7C00.  The image contains:
;     [ loader code ][ 32-bit kernel ][ 64-bit kernel ]
;  The loader:
;     1. enables the A20 line
;     2. reads the BIOS memory map (INT 15h / E820)
;     3. tests the CPU for 64-bit long mode (CPUID)
;     4. copies the VGA ROM font and sets a VBE linear-framebuffer mode
;     5. enters 32-bit protected mode
;     6. 64-bit CPU: builds page tables, enables long mode
;        32-bit CPU: stays in protected mode
;     7. copies the matching kernel to 1 MiB and jumps to it
;
;  Boot info block at 0x600 (dwords):
;     0 magic 'AQUA'   4 boot drive    8 e820 count   12 e820 address
;    16 fb address    20 fb pitch     24 width        28 height
;    32 bpp           36 font address 40 cpu long mode 44 kernel bits 48 vbe ok
; =====================================================================
bits 16
org 0x7C00

BOOTINFO   equ 0x600
E820BUF    equ 0x1000
VBEINFO    equ 0x2000
VBEMODE    equ 0x2200
FONTBUF    equ 0x3000
KERNEL_DST equ 0x100000
PT_BASE    equ 0x1000000

start:
    jmp 0x0000:.go
.go:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000
    sti
    cld
    mov [boot_drive], dl
    mov di, BOOTINFO                    ; clear boot info block
    mov cx, 64
    rep stosw
    mov dword [BOOTINFO], 0x41555141    ; 'AQUA'
    movzx eax, byte [boot_drive]
    mov [BOOTINFO+4], eax

    mov ax, 0x0003                      ; 80x25 text mode
    int 0x10
    mov si, msg_banner
    call puts

    ; ---- A20 ----
    mov si, msg_a20
    call puts
    mov ax, 0x2401                      ; BIOS method
    int 0x15
    in al, 0x92                         ; fast A20
    or al, 2
    and al, 0xFE
    out 0x92, al
    mov si, msg_ok
    call puts
    call pause

    ; ---- memory map ----
    mov si, msg_mem
    call puts
    call get_e820
    mov si, msg_ok
    call puts
    call pause

    ; ---- CPU ----
    mov si, msg_cpu
    call puts
    call check_cpu
    cmp dword [BOOTINFO+40], 0
    je .cpu32
    mov si, msg_cpu64
    jmp .cpudone
.cpu32:
    mov si, msg_cpu32
.cpudone:
    call puts
    call pause

    ; ---- font + video ----
    mov si, msg_font
    call puts
    call get_font
    mov si, msg_ok
    call puts
    mov si, msg_vbe
    call puts
    call pause
    call set_video
    jc video_fail
    mov dword [BOOTINFO+48], 1

    ; ---- protected mode ----
    cli
    lgdt [gdt_ptr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

video_fail:
    mov ax, 0x0003
    int 0x10
    mov si, msg_novbe
    call puts
.h: cli
    hlt
    jmp .h

; ------------------------------------------------------------------
puts:                                   ; DS:SI -> zero-terminated string
    pusha
.l: lodsb
    test al, al
    jz .d
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .l
.d: popa
    ret

pause:                                  ; ~180 ms
    pusha
    mov cx, 0x0002
    mov dx, 0xBF20
    mov ah, 0x86
    int 0x15
    popa
    ret

get_e820:
    xor ebx, ebx
    mov di, E820BUF
    xor bp, bp
.l: mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [es:di+20], 1
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    jcxz .skip
    inc bp
    add di, 24
.skip:
    test ebx, ebx
    jz .done
    cmp bp, 60
    jb .l
.done:
    mov [BOOTINFO+8], bp
    mov dword [BOOTINFO+12], E820BUF
    ret

check_cpu:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no                              ; no CPUID -> 32-bit CPU
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no
    mov dword [BOOTINFO+40], 1
.no:
    ret

get_font:                               ; copy BIOS 8x16 font to FONTBUF
    push es
    push bp
    mov ax, 0x1130
    mov bh, 6
    int 0x10
    push es
    pop ds
    mov si, bp
    xor ax, ax
    mov es, ax
    mov di, FONTBUF
    mov cx, 2048
    cld
    rep movsw
    xor ax, ax
    mov ds, ax
    pop bp
    pop es
    mov dword [BOOTINFO+36], FONTBUF
    ret

set_video:                              ; CF set on failure
    mov di, VBEINFO
    mov dword [di], 'VBE2'
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov ax, [VBEINFO+16]
    mov fs, ax
    xor bx, bx
.target:
    mov ax, [targets+bx]
    mov [want_w], ax
    mov ax, [targets+bx+2]
    mov [want_h], ax
    mov si, [VBEINFO+14]
.mode:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je .next
    add si, 2
    push bx
    push si
    mov [cur_mode], cx
    mov di, VBEMODE
    mov ax, 0x4F01
    int 0x10
    pop si
    pop bx
    cmp ax, 0x004F
    jne .mode
    test byte [VBEMODE], 0x80           ; linear framebuffer supported
    jz .mode
    cmp byte [VBEMODE+0x19], 32
    jne .mode
    cmp byte [VBEMODE+0x1B], 6          ; direct colour
    jne .mode
    mov ax, [VBEMODE+0x12]
    cmp ax, [want_w]
    jne .mode
    mov ax, [VBEMODE+0x14]
    cmp ax, [want_h]
    jne .mode
    jmp .found
.next:
    add bx, 4
    cmp bx, 12
    jb .target
    jmp .fail
.found:
    mov bx, [cur_mode]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov eax, [VBEMODE+0x28]
    mov [BOOTINFO+16], eax
    movzx eax, word [VBEMODE+0x10]
    movzx edx, word [VBEMODE+0x32]
    test edx, edx
    jz .p
    mov eax, edx
.p: mov [BOOTINFO+20], eax
    movzx eax, word [VBEMODE+0x12]
    mov [BOOTINFO+24], eax
    movzx eax, word [VBEMODE+0x14]
    mov [BOOTINFO+28], eax
    mov dword [BOOTINFO+32], 32
    clc
    ret
.fail:
    stc
    ret

; ------------------------------------------------------------------
bits 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7000

    mov esi, kernel32
    mov ecx, k32_size
    mov dword [BOOTINFO+44], 32
    cmp dword [BOOTINFO+40], 0
    je .copy
    mov esi, kernel64
    mov ecx, k64_size
    mov dword [BOOTINFO+44], 64
.copy:
    mov edi, KERNEL_DST
    add ecx, 3
    shr ecx, 2
    cld
    rep movsd

    cmp dword [BOOTINFO+44], 64
    je long_setup
    mov eax, KERNEL_DST
    jmp eax                             ; 32-bit kernel entry

long_setup:
    ; page tables: PML4 -> PDPT -> 4 x PD (2 MiB pages) = 4 GiB identity map
    mov edi, PT_BASE
    xor eax, eax
    mov ecx, (6 * 4096) / 4
    rep stosd
    mov dword [PT_BASE], PT_BASE + 0x1000 + 3
    mov dword [PT_BASE + 0x1000 + 0],  PT_BASE + 0x2000 + 3
    mov dword [PT_BASE + 0x1000 + 8],  PT_BASE + 0x3000 + 3
    mov dword [PT_BASE + 0x1000 + 16], PT_BASE + 0x4000 + 3
    mov dword [PT_BASE + 0x1000 + 24], PT_BASE + 0x5000 + 3
    mov edi, PT_BASE + 0x2000
    mov eax, 0x83                       ; present | write | huge
    mov ecx, 2048
.pd:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .pd
    mov eax, PT_BASE
    mov cr3, eax
    mov eax, cr4
    or eax, 0x20                        ; PAE
    mov cr4, eax
    mov ecx, 0xC0000080                 ; EFER
    rdmsr
    or eax, 0x100                       ; LME
    wrmsr
    mov eax, cr0
    or eax, 0x80000000                  ; paging
    mov cr0, eax
    jmp 0x18:long_entry

bits 64
long_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax
    mov eax, KERNEL_DST
    jmp rax

; ------------------------------------------------------------------
bits 16
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF               ; 0x08 code32
    dq 0x00CF92000000FFFF               ; 0x10 data
    dq 0x00209A0000000000               ; 0x18 code64
gdt_end:
gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

boot_drive dw 0
want_w     dw 0
want_h     dw 0
cur_mode   dw 0
targets    dw 1024, 768, 800, 600, 640, 480

msg_banner db 'AquaOS boot loader', 13, 10, '------------------', 13, 10, 0
msg_a20    db ' Enabling A20 gate ............ ', 0
msg_mem    db ' Reading BIOS memory map ...... ', 0
msg_cpu    db ' Detecting CPU ................ ', 0
msg_font   db ' Loading VGA ROM font .......... ', 0
msg_vbe    db 13, 10, ' Switching to graphics mode (VBE) - starting kernel...', 13, 10, 0
msg_ok     db 'OK', 13, 10, 0
msg_cpu64  db '64-bit CPU -> 64-bit kernel', 13, 10, 0
msg_cpu32  db '32-bit CPU -> 32-bit kernel', 13, 10, 0
msg_novbe  db 'AquaOS: this PC has no VBE linear framebuffer (1024x768x32 / 800x600 / 640x480). Cannot start.', 13, 10, 0

; ------------------------------------------------------------------ kernels
align 512
kernel32:
    incbin "k32.bin"
align 512
k32_end:
kernel64:
    incbin "k64.bin"
align 512
k64_end:
k32_size equ k32_end - kernel32
k64_size equ k64_end - kernel64
