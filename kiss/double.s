        .chip 68030
        .include "kiss/kisshw.s"

        .globl  kiss_check_double_jumper

        .text
        .even

/* KISS-68030 has a hardware jumper named "DOUBLE". Accessing memory above
   64MB when DOUBLE=0 will lead to a bus exception.  When DOUBLE=1 the limit
   is instead 256MB. This routine tries to access memory >64MB and traps
   the resulting bus error (if it occurs) in order to determine the setting
   of the DOUBLE jumper, and thus the maximum possible installed memory. */

kiss_check_double_jumper:
        move.w %sr, -(%sp)              /* save SR including interrupt level */
        or.w #0x0700, %sr               /* force interrupts off */
        movec.l %vbr, %d1               /* save VBR */
        move.l %sp, %a1                 /* save SP in %a1 */
        move.l #(temp_vector-8), %a0    /* load VBR with temporary vector table */
        movec.l %a0, %vbr               /* (only vector 2 will be used) */
        lea 64*1024*1024, %a0           /* load test address */
        move.b (%a0), %d0               /* test it; if DOUBLE=0, the exception takes us to doublefault */
        move.l #1, %d0                  /* if we survive to here, DOUBLE=1 */
        bra.s doubledone
doublefault:
        move.l #0, %d0                  /* if we get here, DOUBLE=0 */
doubledone:
        move.l %a1, %sp                 /* restore SP (discards any exception frame) */
        movec.l %d1, %vbr               /* restore VBR */
        move.w (%sp)+, %sr              /* restore SR including interrupt level */
        rts                             /* return with either 0 or 1 in %d0 */

        /* we use temp_vector-8 as the VBR allowing us to skip the unused first 2 vectors */
        .align 4                        /* vector 0 initial ISP (unused, any junk will do) */
temp_vector:                            /* vector 1 initial PC (unused, any junk will do) */
        .long   doublefault             /* vector 2 access fault / bus error */

        .end
