        .chip 68030
        .include "kiss/kisshw.s"

        .globl  _start
        .globl  gogoboot
        .globl  copyright_msg
        .globl  cpu_cache_disable
        .globl  cpu_cache_flush
        .globl  cpu_cache_invalidate
        .globl  cpu_interrupts_on
        .globl  cpu_interrupts_off
        .globl  vector_table
        .globl  halt

        .section .rom_header
        bra     _start                          /* initial SP - junk - actually a relative jump to _start */
        dc.l    _start + KISS68030_ROM_BASE     /* initial PC - jump to in-ROM address of _start */

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "GOGOBOOT/KISS: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\n\0"

        /* define some space in DRAM for the stack to live */
        .section .stack
        .align 4
stack_bottom:
        .space  (16*1024)
stack_top:

        /* startup code */
        .text
        .even
        /* we're either running from ROM, or we've been loaded in RAM ... somewhere */
        /* we must copy ourselves into place using only PC-relative addresses */
_start:
        move.w #0x2700, %sr             /* setup status register, interrupts off */
        reset

        /* disable, clear all data/instruction cache entries */
        move.l #(CACR_CI + CACR_CD), %d0
        movec.l %d0, %cacr
        nop

        /* flush page translation cache */
        pflusha
        nop

        /* enable data, instruction caches */
        move.l #(CACR_EI + CACR_ED), %d0 
        movec.l %d0, %cacr
        nop

        /* copy .text section into RAM from ROM -- note limited to 256KB */
        lea.l   %pc@(text_start), %a0   /* source address */
        lea.l   text_start, %a1         /* dest address */
        move.l  #(text_size+3), %d0     /* num bytes to copy */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_text
copy_text_loop:
        move.l  (%a0)+,(%a1)+
copy_text:
        dbra    %d0,copy_text_loop

        /* load vector base register */
        lea vector_table, %a0
        movec.l %a0, %vbr

        /* copy .rodata section into RAM from ROM -- note limited to 256KB */
        lea.l   %pc@(rodata_start), %a0   /* source address */
        lea.l   rodata_start, %a1         /* dest address */
        move.l  #(rodata_size+3), %d0     /* num bytes to copy */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_rodata
copy_rodata_loop:
        move.l  (%a0)+,(%a1)+
copy_rodata:
        dbra    %d0,copy_rodata_loop

        /* load .data section into RAM from ROM -- note limited to 256KB */
        lea.l   %pc@(data_start), %a0   /* source address */
        lea.l   data_start, %a1         /* dest address */
        move.l  #(data_size+3), %d0     /* num bytes to copy */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_data
copy_data_loop:
        move.l  (%a0)+,(%a1)+
copy_data:
        dbra    %d0,copy_data_loop

        /* flush (and keep enabled) data, instruction caches */
        move.l #(CACR_EI + CACR_ED + CACR_CI + CACR_CD), %d0 
        movec.l %d0, %cacr
        nop

        /* jump and continue at our target address */
        movea.l #target_address, %a0
        jmp (%a0)
target_address:
        /* phew ... we're done using only PC-relative addresses */

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
        /* disable and clear all data/instruction cache entries */
        movec.l %cacr, %d0
        or.w #(CACR_CI + CACR_CD), %d0
        movec.l %d0, %cacr
        nop
        pflusha
        nop
        rts

cpu_cache_disable:
        /* disable and clear all data/instruction cache entries */
        move.l #(CACR_CI + CACR_CD), %d0
        movec.l %d0, %cacr
        nop
        pflusha
        nop
        rts

cpu_interrupts_on:
        and.w #0xf8ff, %sr
        rts

cpu_interrupts_off:
        or.w #0x0700, %sr
        rts

        .end
