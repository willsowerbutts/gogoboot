	.text
	.globl	_start
	.globl	cboot
	.globl	_exit

        dc.l    0x20000                 /* initial SP: 32KB of RAM between 96KB ROM and 32KB screen 0 */
        dc.l    _start                  /* initial PC */

        .align 16
	.ascii	"Q40BOOT: Copyright (C) 2023 William R. Sowerbutts <will@sowerbutts.com>\r\n"

        .even
_start:
        move.w #0x2700, %sr
        reset

        /* should probably set up the various 040 cache registers here? */
        /* should clear BSS */
        /* should copy initialised data from ROM */

        jsr cboot                       /* off to C land */

        st 0xff000030                   /* turn LED on plz */

        /* halt */
_exit:  stop #0x2701                    /* all done */
        br.s _exit                      /* loop on NMI */
        .end
