OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY 
{
    rom    : ORIGIN = 0x00000000, LENGTH = 96K     /* first 96KB only; full 256KB is at 0xFE000000 */
    ram    : ORIGIN = 0x00028000, LENGTH = 3936K   /* assume a minimum 4MB RAM system */
}

SECTIONS
{
    .text : { 
        text_start = .;
        *(.text.unlikely .text.*_unlikely .text.unlikely.*)
        *(.text.exit .text.exit.*)
        *(.text.startup .text.startup.*)
        *(.text.hot .text.hot.*)
        *(SORT(.text.sorted.*))
        *(.text .stub .text.* .gnu.linkonce.t.*)
        text_end = .;
    } >rom
    text_size = SIZEOF(.text);

    /* .rodata : AT(text_end) { */
    .rodata : { 
        rodata_start = .;
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        rodata_end = .;
    } >rom 
    rodata_size = SIZEOF(.rodata);

    /* .data : AT(rodata_end) {  */
    .data : { 
        data_start = .;
        *(.data .data.* .gnu.linkonce.d.*)
        data_end = .;
    } >ram AT>rom
    data_size = SIZEOF(.data);
    data_load_start = LOADADDR(.data);

    .bss : { 
        bss_start = .;
        *(.dynbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        bss_end = .;
    } >ram
    bss_size = SIZEOF(.bss);
}
