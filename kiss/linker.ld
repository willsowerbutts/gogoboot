OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY 
{
    rom    : ORIGIN = 0xFFF00000, LENGTH = 512K
    ram    : ORIGIN = 0x00000000, LENGTH = 4096K   /* assume a minimum 4MB RAM system */
}

SECTIONS
{
    .text : { 
        text_start = .;
        *(.rom_header)
        *(.text.unlikely SORT(.text.*_unlikely) SORT(.text.unlikely.*))
        *(.text.exit SORT(.text.exit.*))
        *(.text.startup SORT(.text.startup.*))
        *(.text.hot SORT(.text.hot.*))
        *(SORT(.text.sorted.*))
        *(.text .stub)
        *(SORT(.text.*) SORT(.gnu.linkonce.t.*))
        text_end = .;
    } >rom
    text_size = SIZEOF(.text);

    .rodata : { 
        rodata_start = .;
        *(.rodata SORT(.rodata.*) SORT(.gnu.linkonce.r.*))
        rodata_end = .;
    } >rom 
    rodata_size = SIZEOF(.rodata);

    .stack : {
        stack_start = .;
        *(.stack SORT(.stack.*))
        stack_end = .;
    } >ram
    stack_size = SIZEOF(.stack);

    .data stack_end : { 
        data_start = .;
        *(.data SORT(.data.*) SORT(.gnu.linkonce.d.*))
        data_end = .;
    } >ram AT>rom
    data_size = SIZEOF(.data);
    data_load_start = LOADADDR(.data);

    .bss data_end : { 
        bss_start = .;
        *(.dynbss)
        *(.bss SORT(.bss.*) SORT(.gnu.linkonce.b.*))
        *(COMMON)
        bss_end = .;
    } >ram
    bss_size = SIZEOF(.bss);
}
