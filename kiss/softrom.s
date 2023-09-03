        .include "kiss/kisshw.s"

        .globl  kiss_boot_softrom

        .section .text
        .align 4

kiss_boot_softrom:
        /* enter with interrupts on, cpu caches enabled */
        move.w #0x2700, %sr             /* setup status register -- interrupts off */
        movea.l %sp@(4), %a0            /* a0 = pointer to new softrom image  */

        /* disable and clear all data/instruction cache entries */
        move.l #(CACR_CI + CACR_CD), %d0
        movec.l %d0, %cacr

        nop

        pflusha
        nop

        jmp %a0@                        /* ... and off we go! */
copyend:
        /* this is where the code copying ends */
        .end
