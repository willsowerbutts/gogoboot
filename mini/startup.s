        .chip 68000
        .include "mini/minihw.s"

        .globl  _start
        .globl  gogoboot
        .globl  copyright_msg
        .globl  cpu_cache_disable
        .globl  cpu_cache_flush
        .globl  cpu_cache_invalidate
        .globl  cpu_interrupts_on
        .globl  cpu_interrupts_off
        .globl  halt

        .section .rom_header
rom_start:
        /* initial SP: actually contain a relative jump to _start, we load SP manually later */
        /* this is instead used when we "softrom" the ROM image */
        bra     _start                          
        /* initial PC - jump to ROM address of _start */
        /* this is used at machine power on */
        dc.l    _start - rom_start + MINI68K_ROM_BASE  

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "GOGOBOOT/MINI: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\0"

        /* define some space in DRAM for the stack to live */
        .section .stack
        .align 4
stack_bottom:
        .space  (8*1024)
stack_top:

        /* startup code */
        .section .text
        .even
_start:
        move.w #0x2700, %sr             /* setup status register, interrupts off */
        reset

        /* copy .data section into RAM from ROM -- note limited to 256KB */
        lea.l   data_load_start, %a0    /* source address */
        lea.l   data_start, %a1         /* dest address */
        /* there must be a better way for the assembler/linker to compute how many longwords to copy! */
        move.l  #(data_size+3), %d0     /* last byte to copy (round up to next longword) */
        sub.l   %a1, %d0                /* subtract start address */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_data
copy_data_loop:
        move.l  (%a0)+,(%a1)+
copy_data:
        dbra    %d0,copy_data_loop

        /* set stack pointer */
        lea stack_top, %sp

        /* clear the .bss section -- note limited to 256KB */
        lea.l   bss_start, %a1
        move.l  #(bss_size+3), %d0      /* num bytes to zap; round up  */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    zap_bss
zap_bss_loop:
        clr.l   (%a1)+
zap_bss:
        dbra    %d0, zap_bss_loop

        jsr gogoboot                    /* off to C land */

        /* halt */
halt:
stopped:
        stop #0x2700                    /* all done */
        br.s stopped                    /* loop on NMI */

cpu_cache_flush:
cpu_cache_invalidate:
cpu_cache_disable:
        rts

cpu_interrupts_on:
        and.w #0xf8ff, %sr
        rts

cpu_interrupts_off:
        or.w #0x0700, %sr
        rts

        .end
