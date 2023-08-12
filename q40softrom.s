        .chip 68040

        .globl  q40_boot_softrom

        .section .text
        .align 4

        /* 
         * Q40 supports "ROM emulation". The low 96KB of RAM works in two modes:
         * "Normal mode": reads are directed to the ROM at 0xFE000000, 
         *                writes are directed to the RAM at 0x00000000.
         * "LowRAM mode": reads are directed to the RAM at 0x00000000,
         *                writes are discarded.
         * This allows us to use RAM in place of ROM for software development
         */

q40_boot_softrom:
        /* enter with interrupts off, but cpu caches enabled */
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop
        movea.l %sp@(4), %a2            /* a2 = pointer to new softrom image */
        /* We might be running in LowRAM mode already. We need to turn off LowRAM
           to write the new ROM image, so we can't execute our code from LowRAM
           memory. Make a copy elsewhere in RAM and execute from there. Code has
           to be position independent. Since we're not going back to C , I chose
           to overwrite the .data and .bss segments. */

        lea.l data_start, %a0           /* a0 = target pointer */
        lea.l copystart, %a1            /* a1 = source pointer */
        /* compute d0 = routine length in dwords, -1 as we don't skip over the first move */
        move.l #((copyend-copystart+3)/4)-1, %d0
nextword:
        move.l (%a1)+, (%a0)+           /* copy routine into place */
        dbra %d0, nextword
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop
        move.w #0x2700, %sr             /* setup status register -- interrupts off */
        jmp data_start                  /* continue execution but in new location */

        /* below here is the code that is copied out of ROM to .data at the start of RAM */
copystart:
        moveb #1,0xff010000             /* disable LowRAM mode */
        nop                             /* ... real ROM now mapped at address 0 */
        nop                             /* ... writes to low 96KB go to underlying RAM */
        lea.l 0, %a0                    /* a0 = target pointer */
        move.l #0x5fff, %d0             /* copy 96KB in dwords = 4 x (0x5fff+1) */
romnextword:
        move.l (%a2)+, (%a0)+           /* copy ROM image into place in low RAM */
        dbra %d0, romnextword
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop
        moveq #0, %d0
        movec %d0, %vbr                 /* clear VBR */
        movec %d0, %cacr                /* clear CPU cache */
        movec %d0, %tc                  /* clear MMU translation control */
        movec %d0, %itt0                /* clear MMU transparent translation */
        movec %d0, %itt1                /* clear MMU transparent translation */
        movec %d0, %dtt0                /* clear MMU transparent translation */
        movec %d0, %dtt1                /* clear MMU transparent translation */
        moveq #14, %d0
        movea.l #0xff000000, %a0        /* clear 15 MASTER CPLD registers */
nextregister:                           /* at 0xff000000 -- 0xff000038 */
        sf (%a0)                        /* byte write to clear register */
        addq #4, %a0                    /* registers are every 4 bytes */
        dbra %d0, nextregister          /* loop until done */
        moveb #1,0xff018000             /* enable LowRAM mode */
        nop                             /* ... RAM replaces ROM at address 0 */
        nop                             /* (and it is write protected) */
        moveal 0x0, %sp                 /* load initial SP - vector 0 */
        /* The Q40 SOFTROM program does this next; I'm not sure why
        /*   movel %sp, %d0                        SP
        /*   andiw #0x8000, %d0                   ... & 0xFFFF8000
        /*   moveal %d0, %fp                      -> FP (A6)
        /*   clrl %fp@                     *FP = 0 (WRS: why??)
        /* I don't understand why they do this, and for my ROM it just
        /* scribbles on video RAM, so I won't replicate it */
        moveal 0x4, %a0                 /* load initial PC - vector 1 */
        jmp %a0@                        /* ... and off we go! */
copyend:
        /* this is where the code copying ends */
        .end
