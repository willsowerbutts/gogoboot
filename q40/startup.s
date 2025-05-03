        .include "core/cpu-68040-bits.s"
        .globl  _start
        .globl  gogoboot
        .globl  halt
        .globl  copyright_msg
        .globl  vector_table
        .globl  measure_ram_size
        .globl  stack_top

        .section .rom_header
        dc.l    0x20000                 /* initial SP: 32KB of RAM between low ROM and "screen 0" video RAM */
        dc.l    _start                  /* initial PC */

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "GogoBoot/q40: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\0"

        .section .text
        .even
_start:
        move.w #0x2700, %sr             /* setup status register, interrupts off */
        reset

        /* load vector base register */
        lea vector_table, %a0
        movec %a0, %vbr

        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop

        moveq #0, %d0
        movec %d0, %cacr                /* clear CPU cache control */
        movec %d0, %tc                  /* clear MMU translation control */
        movec %d0, %itt0                /* clear MMU transparent translation */
        movec %d0, %itt1                /* clear MMU transparent translation */
        movec %d0, %dtt0                /* clear MMU transparent translation */
        movec %d0, %dtt1                /* clear MMU transparent translation */

        moveq #14, %d0                  /* clear 15 MASTER CPLD registers */
        movea.l #0xff000000, %a0        /* at 0xff000000 -- 0xff000038 */
nextregister:
        sf (%a0)                        /* byte write to clear register */
        addq #4, %a0                    /* registers are every 4 bytes */
        dbra %d0, nextregister          /* loop until done */

        /* setup the 040 transparent translation / cache registers 
           https://www.nxp.com/docs/en/reference-manual/MC68040UM.pdf page 3-5 
             bit 15     e  = enable
             bit 13,14  s  = user/supervisor mode (10=both)
             bit 5,6    cm = 00 cached, write-through
                             01 cached, copy-back
                             10 non-cached, serialised
                             11 non-cached
           data+instr ttr0: 0xff00_0000 -- 0xffff_ffff (top 16MB) = noncached, serialised
           data+instr ttr1: 0x0000_0000 -- 0xffff_ffff (all 4GB)  = cached, copyback
           if both match, ttr0 takes priority over ttr1 */

        movel #0xff00c040, %d0 /* match=ff, mask=00, e=1, s=10, cm=10 */
        movec %d0, %dtt0
        movec %d0, %itt0
        nop
        movel #0x00ffc020, %d0 /* match=00, mask=ff, e=1, s=10, cm=01 */
        movec %d0, %dtt1
        movec %d0, %itt1
        nop

        /* motorola says do a pflush after messing with TTRs */
        pflusha
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

        lea bss_end+256, %sp            /* use temporary stack (after .bss) */
        jsr measure_ram_size            /* call C helper */
        movea.l stack_top, %sp          /* move stack to top of RAM */
        move.l #(CACR_EI+CACR_ED), %d0 /* enable data, instruction caches */
        movec %d0, %cacr
        nop
        jsr gogoboot                    /* call C boot code */

        /* halt */
halt:
halted: stop #0x2700                    /* all done */
        br.s halted                     /* loop on NMI */

        .end
