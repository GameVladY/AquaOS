ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text : { *(.text.entry) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    . = ALIGN(16);
    __bss_start = .;
    .bss : { *(.bss .bss.*) *(COMMON) }
    . = ALIGN(16);
    __bss_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) *(.note.gnu.property) }
}
