        .chip 68030
        .include "kiss/kisshw.s"

        .globl  _start
        .globl  boot_kiss
        .globl  copyright_msg
        .globl  cpu_cache_disable
        .globl  cpu_cache_flush
        .globl  cpu_cache_invalidate
        .globl  cpu_interrupts_on
        .globl  cpu_interrupts_off
        .globl  vector_table
        .globl  halt
        .globl  kiss_check_double_jumper

        .section .rom_header
        dc.l    stack_top               /* initial SP: 32KB of RAM */
        dc.l    _start                  /* initial PC */

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
        .align 16
stack_bottom:
        .space  (16*1024)
stack_top:

        .text
        .even
_start:
        move.w #0x2700, %sr             /* setup status register, interrupts off */
        reset

        /* load vector base register */
        lea vector_table, %a0
        movec.l %a0, %vbr

        /* disable and clear all data/instruction cache entries */
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

        /* load .data section into RAM from ROM -- note limited to 256KB */
        lea.l   data_load_start, %a0    /* source address */
        lea.l   data_start, %a1         /* dest address */
        move.l  #(data_size+3), %d0     /* num bytes to copy */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    copy_data
copy_loop:
        move.l  (%a0)+,(%a1)+
copy_data:
        dbra    %d0,copy_loop

        /* clear the .bss section -- note limited to 256KB */
        lea.l   bss_start, %a1
        move.l  #(bss_size+3), %d0      /* num bytes to zap; round up  */
        lsr.l   #2, %d0                 /* convert to longwords (div 4) */
        br.s    zap_bss
zap_loop:
        clr.l   (%a1)+
zap_bss:
        dbra    %d0, zap_loop

        jsr boot_kiss                   /* off to C land */

        /* halt */
halt:   stop #0x2700                    /* all done */
        br.s halt                       /* loop on NMI */

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

/* KISS-68030 has a hardware jumper named "DOUBLE". Accessing memory above 
   64MB when DOUBLE=0 will lead to a bus exception.  When DOUBLE=1 the limit 
   is instead 256MB. This routine tries to access memory >64MB and traps
   the resulting bus error (if it occurs) in order to determine the setting
   of the DOUBLE jumper, and thus the maximum possible installed memory. */

        .align 4                        /* vector 0 (unspecified) initial ISP */
temp_vectors:                           /* vector 1 (unspecified) initial PC */
        .long   doublefault             /* vector 2 (specified)   access fault / bus error */

kiss_check_double_jumper:
        move.w %sr, -(%sp)              /* save SR including interrupt level */
        or.w #0x0700, %sr               /* force interrupts off */
        movec.l %vbr, %d1               /* save VBR */
        move.l %sp, %a1                 /* save SP in %a1 */
        move.l #(temp_vectors-8), %a0   /* load VBR with temporary vector table */
        movec.l %a0, %vbr               /* (only vector 2 can be used) */
        lea 64*1024*1024, %a0           /* load test address */
        move.b (%a0), %d0               /* test it */
        move.l #256, %d0                /* if we get here, DOUBLE=1 */
        bra.s doubledone
doublefault:
        move.l #64, %d0                 /* if we get here, DOUBLE=0 */
doubledone:
        move.l %a1, %sp                 /* restore SP */
        movec.l %d1, %vbr
        move.w (%sp)+, %sr              /* restore SR including interrupt level */
        rts                             /* return with value in %d0 */

        .end
