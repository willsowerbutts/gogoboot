OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

MEMORY 
{
    rom      : ORIGIN = 0x00000000, LENGTH = 96K     /* first 96KB only; full 256KB is at 0xFE000000 */
    qlram    : ORIGIN = 0x00018000, LENGTH = 24K     /* 32KB - lower portion for .data/.bss */
    stackram : ORIGIN = 0x0001e000, LENGTH = 8K      /* 32KB - upper portion for 8KB stack */
    bigram   : ORIGIN = 0x00028000, LENGTH = 3936K   /* assume a minimum 4MB RAM system */
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

    /* .rodata : AT(text_end) { */
    .rodata : { 
        rodata_start = .;
        *(.rodata SORT(.rodata.*) SORT(.gnu.linkonce.r.*))
        rodata_end = .;
    } >rom 
    rodata_size = SIZEOF(.rodata);

    /* .data : AT(rodata_end) {  */
    .data : { 
        data_start = .;
        *(.data SORT(.data.*) SORT(.gnu.linkonce.d.*))
        data_end = .;
    } >qlram AT>rom
    data_size = SIZEOF(.data);
    data_load_start = LOADADDR(.data);

    .bss : { 
        bss_start = .;
        *(.dynbss)
        *(.bss SORT(.bss.*) SORT(.gnu.linkonce.b.*))
        *(COMMON)
        bss_end = .;
    } >qlram
    bss_size = SIZEOF(.bss);

    .stack : {
        stack_start = .;
        *(.stack SORT(.stack.*))
        stack_end = .;
    } >stackram
    stack_size = SIZEOF(.stack);
}
