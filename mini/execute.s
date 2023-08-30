        .globl  machine_execute

        .section .text
        .align 4
machine_execute:
        /* enter with interrupts off, cpu caches enabled */
        move.w #0x2700, %sr             /* force status register: interrupts off */
        movea.l %sp@(4), %a2            /* a2 = pointer to entry vector */
        jsr %a2@                        /* ... and off we go! */
        rts                             /* feels unlikely we're coming back, but ... */
        .end
