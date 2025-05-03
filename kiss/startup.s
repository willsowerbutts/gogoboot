        .include "core/cpu-68030-bits.s"
        .include "kiss/kisshw.s"

        .globl  _start
        .globl  gogoboot
        .globl  copyright_msg
        .globl  vector_table
        .globl  measure_ram_size
        .globl  stack_top
        .globl  halt

        .section .rom_header
rom_start:
        /* initial SP: actually contain a relative jump to _start, we load SP manually later */
        /* this is instead used when we "softrom" the ROM image */
        bra     _start
        /* initial PC - jump to ROM address of _start */
        /* this is used at machine power on */
        dc.l    hwreset - rom_start + KISS68030_ROM_BASE

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "GogoBoot/kiss: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\0"

        /* startup code */
        .section .text
        .even
hwreset:
        /* ROM reset vector: it is safe to pulse RESET when running from ROM. when
           running from RAM we avoid doing this, because U304 causes the next few 
           memory accesses to come from ROM even if we're actually running from RAM */
        reset
_start:
        move.w #0x2700, %sr             /* setup status register, interrupts off */

        /* we're either running from ROM, or we've been loaded in RAM ... somewhere */
        /* we must copy ourselves into place using only PC-relative addresses */

        /* disable, clear all data/instruction cache entries */
        move.l #(CACR_CI + CACR_CD), %d0
        movec.l %d0, %cacr
        nop

        /* flush page translation cache */
        pflusha
        nop

        /* copy .text+.rodata+.data sections into RAM from ROM -- note limited to 256KB */
        lea.l   %pc@(text_start), %a0   /* source address - PC relative */
        movea.l #text_start, %a1        /* dest address */

        cmpa.l %a0, %a1                 /* compare */
        beq.s target_address            /* skip copying if we're in place already */

        /* there must be a better way for the assembler/linker to compute how many longwords to copy! */
        move.l  #(data_end+3), %d0      /* last byte to copy (round up to next longword) */
        sub.l   %a1, %d0                /* subtract start address */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_text
copy_text_loop:
        move.l  (%a0)+,(%a1)+
copy_text:
        dbra    %d0,copy_text_loop

        /* jump and continue at our target address */
        movea.l #target_address, %a0
        jmp (%a0)
target_address:
        /* phew ... we're done using only PC-relative addresses */

        /* load vector base register */
        lea vector_table, %a0
        movec.l %a0, %vbr

        /* clear the .bss section -- note limited to 256KB */
        lea.l   bss_start, %a1
        move.l  #(bss_size+3), %d0      /* num bytes to zap; round up  */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    zap_bss
zap_bss_loop:
        clr.l   (%a1)+
zap_bss:
        dbra    %d0, zap_bss_loop

        lea bss_end+256, %sp            /* use temporary stack (after .bss) */
        jsr measure_ram_size            /* call C helper */
        movea.l stack_top, %sp          /* move stack to top of RAM */
        move.l #(CACR_EI + CACR_ED), %d0 
        movec.l %d0, %cacr              /* enable data, instruction caches */
        nop
        jsr gogoboot                    /* call C boot code */

        /* halt */
halt:
stopped:
        stop #0x2700                    /* all done */
        br.s stopped                    /* loop on NMI */

        .end
