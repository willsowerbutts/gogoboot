        .text
        .globl  _start
        .globl  cboot
        .globl  copyright_msg

        dc.l    0x20000                 /* initial SP: 32KB of RAM between 96KB ROM and 32KB screen 0 */
        dc.l    _start                  /* initial PC */

        /* get this copyright message up near the head of the ROM */
        .align 16
copyright_msg:
        .ascii  "Q40BOOT: Copyright (c) 2023 William R. Sowerbutts <will@sowerbutts.com>\n\n"
        .ascii  "This program is free software: you can redistribute it and/or modify it under\n"
        .ascii  "the terms of the GNU General Public License as published by the Free Software\n"
        .ascii  "Foundation, either version 3 of the License, or (at your option) any later\n"
        .ascii  "version.\n\0" /* ensure string is terminated */

        .even
_start:
        move.w #0x2700, %sr
        reset

        /* should probably set up the various 040 cache registers here? */
        /* should clear BSS */
        /* should copy initialised data from ROM */

        jsr cboot                       /* off to C land */

        /* halt */
halted: stop #0x2701                    /* all done */
        br.s halted                     /* loop on NMI */
        .end
