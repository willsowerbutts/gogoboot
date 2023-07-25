        .chip 68040
        .text

        .globl  _start
        .globl  boot_q40
        .globl  copyright_msg
        .globl  cpu_cache_disable

        dc.l    0x20000                 /* initial SP: 32KB of RAM between low ROM and "screen 0" video RAM */
        dc.l    _start                  /* initial PC */

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "Q40BOOT: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\n\0"

        .even
_start:
        move.w #0x2700, %sr
        reset

        cpusha %bc              /* write back and invalidate all data/instruction cache entries */
        nop

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

        move.l #0x80008000, %d0 /* enable data, instruction caches */
        movec %d0, %cacr
        nop

        /* load .data section into RAM from ROM */
        lea.l   data_load_start, %a0    /* source address */
        lea.l   data_start, %a1         /* dest address */
        move.l  data_size, %d0          /* num bytes to copy */
        br.s    copy_data
copy_loop:
        move.b  (%a0)+,(%a1)+
copy_data:
        dbra    %d0,copy_loop

        /* clear the .bss section */
        lea.l   bss_start, %a1
        move.l  bss_size, %d0            /* num bytes to zap  */
        br.s    zap_bss
zap_loop:
        clr.b   (%a1)+
zap_bss:
        dbra    %d0, zap_loop

        jsr boot_q40                    /* off to C land */

        /* halt */
halted: stop #0x2700                    /* all done */
        br.s halted                     /* loop on NMI */

cpu_cache_disable:
        cpusha %bc              /* write back and invalidate all data/instruction cache entries */
        nop
        pflusha
        nop
        move.l #0x00000000, %d0 /* disable data, instruction caches */
        movec %d0, %cacr
        nop
        rts

        .end
