	.text
	.globl	_start

        dc.l    0x20000                 /* initial SP: 32KB of RAM between 96KB ROM and 32KB screen 0 */
        dc.l    _start                  /* initial PC */

        .align 16
	.ascii	"Q40BOOT: Copyright (C) 2023 William R. Sowerbutts <will@sowerbutts.com>\r\n"


        .align 4
_start:
        move.w #0x2700, %sr
        bra.s _start

        .end
