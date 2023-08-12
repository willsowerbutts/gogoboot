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
        /* enter with interrupts and cpu caches disabled */
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        movea.l %sp@(4), %a2            /* a2 = pointer to new softrom image */
        /* we might be running in a soft ROM already, so we need to copy ourselves
           before we copy the new image in case we overwrite ourselves. The 32KB
           RAM at 96KB seems a sensible place to choose. */
        lea.l 0x18000, %a0              /* a0 = target pointer */
        lea.l copystart, %a1            /* a1 = source pointer */
        /* compute d0 = routine length in dwords, -1 as we don't skip over the first move */
        move.l #((copyend-copystart+3)/4)-1, %d0
nextword:
        move.l (%a1)+, (%a0)+           /* copy routine into place */
        dbra %d0, nextword
        cpusha %bc                      /* write back and invalidate all data/instruction cache entries */
        nop
        jmp 0x18000                     /* continue execution but in new location */
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
        /* the remainder of this routine is largely based on the original Q40 "SOFTROM" */
        moveq #0, %d0
        movec %d0, %vbr                 /* reset VBR */
        movec %d0, %cacr                /* reset CPU cache */
        movec %d0, %tc                  /* reset MMU translation control */
        movec %d0, %itt0                /* reset MMU transparent translation */
        movec %d0, %itt1                /* reset MMU transparent translation */
        movec %d0, %dtt0                /* reset MMU transparent translation */
        movec %d0, %dtt1                /* reset MMU transparent translation */
        moveq #14, %d0
        movea.l #0xff000000, %a0        /* clear 15 MASTER CPLD registers */
nextregister:                           /* at 0xff000000 -- 0xff000038 */
        moveb #0, (%a0)                 /* byte write to register */
        addq #4, %a0                    /* registers are every 4 bytes */
        dbra %d0, nextregister          /* loop until done */
        moveb #1,0xff018000             /* enable LowRAM mode */
        nop                             /* ... RAM replaces ROM at address 0 */
        nop                             /* (and it is write protected) */
        moveal 0x0, %sp                 /* load initial SP */
        /* The Q40 SOFTROM program does this next; I'm not sure why */
        movel %sp, %d0                  /* FP = SP & 0xFFFF8000 */
        andiw #0x8000, %d0
        moveal %d0, %fp
        clrl %fp@                       /* *FP = 0 (WRS: why?) */
        /* End mysterious code. It proceeds just as one might expect: */
        moveal 0x4, %a0                 /* load initial PC */
        jmp %a0@                        /* and off we go! */
copyend:
        .end
