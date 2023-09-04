OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY 
{
    rom    : ORIGIN = 0x380000, LENGTH = 448K
    ram    : ORIGIN = 0x000000, LENGTH = 2048K   /* assume a minimum 4MB RAM system */
}

SECTIONS
{
    .text : { 
        text_start = .;
        *(.vectors SORT(.vectors.*)) /* must come first -- has to be at address 0 */
        *(.rom_header)
        *(.text.unlikely SORT(.text.*_unlikely) SORT(.text.unlikely.*))
        *(.text.exit SORT(.text.exit.*))
        *(.text.startup SORT(.text.startup.*))
        *(.text.hot SORT(.text.hot.*))
        *(SORT(.text.sorted.*))
        *(.text .stub)
        *(SORT(.text.*) SORT(.gnu.linkonce.t.*))
        text_end = .;
    } >ram
    text_size = SIZEOF(.text);

    .rodata : { 
        rodata_start = .;
        *(.rodata SORT(.rodata.*) SORT(.gnu.linkonce.r.*))
        rodata_end = .;
    } >ram 
    rodata_size = SIZEOF(.rodata);

    .data : { 
        data_start = .;
        *(.data SORT(.data.*) SORT(.gnu.linkonce.d.*))
        data_end = .;
    } >ram
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
