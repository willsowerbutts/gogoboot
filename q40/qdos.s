        .globl  q40_boot_qdos

        .section .text
        .align 4
q40_boot_qdos:
        /* enter with interrupts on, cpu caches enabled */
        move.w #0x2700, %sr             /* setup status register -- interrupts off */
        movea.l %sp@(4), %a2            /* a2 = pointer to entry vector */
        moveq #0, %d0
        /* movec %d0, %vbr */           /* DO NOT clear VBR */
        movec %d0, %cacr                /* clear CPU cache control */
        movec %d0, %tc                  /* clear MMU translation control */
        movec %d0, %itt0                /* clear MMU transparent translation */
        movec %d0, %itt1                /* clear MMU transparent translation */
        movec %d0, %dtt0                /* clear MMU transparent translation */
        movec %d0, %dtt1                /* clear MMU transparent translation */
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop
        moveq #14, %d0                  /* clear 15 MASTER CPLD registers */
        movea.l #0xff000000, %a0        /* at 0xff000000 -- 0xff000038 */
nextregister:
        sf (%a0)                        /* byte write to clear register */
        addq #4, %a0                    /* registers are every 4 bytes */
        dbra %d0, nextregister          /* loop until done */
        jmp %a2@                        /* ... and off we go! */
        .end
