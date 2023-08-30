        .globl  machine_execute
        .globl  cpu_cache_invalidate

        .section .text
        .align 4
machine_execute:
        /* enter with interrupts off, cpu caches enabled */
        move.w #0x2700, %sr             /* force status register: interrupts off */
        jsr cpu_cache_invalidate        /* flush cache */
        movea.l %sp@(4), %a2            /* a2 = pointer to entry vector */
        /* moveq #0, %d0 */
        /* movec %d0, %vbr */           /* DO NOT clear VBR */
        /* movec %d0, %cacr */          /* DO NOT clear CPU cache control */
        /* movec %d0, %tc   */          /* DO NOT clear MMU translation control */
        /* movec %d0, %itt0 */          /* DO NOT clear MMU transparent translation */
        /* movec %d0, %itt1 */          /* DO NOT clear MMU transparent translation */
        /* movec %d0, %dtt0 */          /* DO NOT clear MMU transparent translation */
        /* movec %d0, %dtt1 */          /* DO NOT clear MMU transparent translation */
        moveq #14, %d0                  /* clear 14+1 = 15 MASTER CPLD registers */
        movea.l #0xff000000, %a0        /* at 0xff000000 -- 0xff000038 */
nextregister:
        sf (%a0)                        /* byte write to clear register */
        addq #4, %a0                    /* registers are every 4 bytes */
        dbra %d0, nextregister          /* loop until done */
        jsr %a2@                        /* ... and off we go! */
        rts                             /* feels unlikely we're coming back, but ... */
        .end
