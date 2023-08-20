        .chip 68030

        .globl  ide_sector_xfer_input
        .globl  ide_sector_xfer_output

        .text
        .even

ide_sector_xfer_input:
    moveal %sp@(4),%a0          /* void *buf */
    moveal %sp@(8),%a1          /* 8255 base address */

    /* save registers */
    movem.l %d2-%d3/%a2,-(%sp)
    
    lea %a1@(3),%a2             /* 8255 control register */
    moveq #13, %d2
    moveq #12, %d3
    move.l #127, %d1            /* total 128x4=512 bytes */

ide_input_nextword:
    /* read a DWORD */
    moveb %d2, %a2@             /* begin /RD pulse */
    movew %a1@, %d0             /* reads LSB then MSB in that order */
    moveb %d3, %a2@             /* end /RD pulse */
    swap %d0                    /* move to the top half of the word */
    moveb %d2, %a2@             /* begin /RD pulse */
    movew %a1@, %d0             /* reads LSB then MSB in that order */
    moveb %d3, %a2@             /* end /RD pulse */

    /* store to memory */
    movel %d0, %a0@+

    /* loop until done */
ide_input_loop:
    dbra %d1, ide_input_nextword

    /* restore registers, return */
    movem.l (%sp)+,%d2-%d3/%a2
    rts


ide_sector_xfer_output:
    moveal %sp@(4),%a0          /* void *buf */
    moveal %sp@(8),%a1          /* 8255 base address */

    /* save registers */
    movem.l %d2-%d3/%a2,-(%sp)
    
    lea %a1@(3),%a2             /* 8255 control register */
    moveq #11, %d2
    moveq #10, %d3
    move.l #127, %d1            /* total 128x4=512 bytes */

    /* note that to give the drive time to latch the data we do
       housekeeping including swaps and branches while the /WR
       line is asserted -- so the first thing we do each loop is
       end the previous cycle's /WR pulse, which is a NOP on the
       entry to the first loop */

ide_output_nextword:
    /* load from memory */
    movel %a0@+, %d0
    /* write a DWORD */
    moveb %d3, %a2@             /* end /WR pulse -- nop on the first loop*/
    swap %d0                    /* top half first */
    movew %d0, %a1@             /* set up data lines */
    moveb %d2, %a2@             /* begin /WR pulse */
    swap %d0                    /* get the bottom half */
    moveb %d3, %a2@             /* end /WR pulse */
    movew %d0, %a1@             /* set up data lines */
    moveb %d2, %a2@             /* begin /WR pulse */
    /* loop until done */
ide_output_loop:
    dbra %d1, ide_output_nextword

    moveb %d3, %a2@          /* end the final /WR pulse */

    /* restore registers, return */
    movem.l (%sp)+,%d2-%d3/%a2
    rts
        .end
