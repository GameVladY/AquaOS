; AquaOS 32-bit kernel entry + interrupt stubs (NASM, elf32)
bits 32
section .text.entry
global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli
    cld
    mov esp, 0x1200000
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kmain
.halt:
    cli
    hlt
    jmp .halt

section .text
extern exception_handler
extern timer_tick

%macro EXC_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro
%macro EXC_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

isr_common:
    cld
    mov eax, esp
    push eax
    call exception_handler
.h: cli
    hlt
    jmp .h

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

global irq0_stub
irq0_stub:
    pushad
    cld
    call timer_tick
    popad
    iret

global irq_ignore
irq_ignore:
    iret

section .data
global isr_table
isr_table:
%assign i 0
%rep 32
    dd isr %+ i
%assign i i+1
%endrep
