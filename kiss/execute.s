        .include "kiss/kisshw.s"

        .globl  machine_execute
        .globl  loader_bounce_buffer_data
        .globl  loader_bounce_buffer_size
        .globl  loader_bounce_buffer_target
        .globl  loader_scratch_space

        .section .text
        .align 4
machine_execute:
        move.w #0x2700, %sr                             /* force status register: interrupts off */
        movea.l %sp@(4), %a5                            /* a5 = pointer to entry vector */
        movea.l %sp@(8), %sp                            /* update stack pointer */
        movea.l (loader_scratch_space), %a0             /* a0 = target for our copy/jump routine */
        movea.l (loader_bounce_buffer_data), %a1        /* a1 = bounce buffer source addr */
        movea.l (loader_bounce_buffer_target), %a2      /* a2 = bounce buffer target addr */
        move.l (loader_bounce_buffer_size), %d0         /* d0 = bounce buffer size */
        cmp.l #0, %d0                                   /* test size == 0? */
        beq.s runit                                     /* not in use? skip copying */
        /* we're going to overwrite this code (potentially), so we need to execute from some scratch space */
        lea.l copystart, %a3                            /* a3 = source pointer */
        /* compute d1 = routine length in dwords, -1 as we don't skip over the first move */
        move.l #((copyend-copystart+3)/4)-1, %d1
nextword:
        move.l (%a3)+, (%a0)+                           /* copy routine into place */
        dbra %d1, nextword
        /* clear all data/instruction cache entries */
        /* does not change if cache enabled/disabled */
        movec.l %cacr, %d1
        or.w #(CACR_CI + CACR_CD), %d1
        movec.l %d1, %cacr
        nop
        movea.l (loader_scratch_space), %a0             /* a0 = target for our copy/jump routine */
        jmp (%a0)                                       /* continue execution in new location */

        /* code below this point is copied to a scratch buffer */
        /* WARNING: the scratch buffer is only 128 bytes in size! */

copystart:
        addq #3, %d0                                    /* round up size (although it should be in whole dwords already) */
        lsr.l #2, %d0                                   /* covert to longwords (div 4) */
        br.s bounce_copy
bounce_loop:
        move.l (%a1)+,(%a2)+
bounce_copy:
        dbra %d0, bounce_loop
        /* bounce buffer is now copied into place */
runit:
        /* clear all data/instruction cache entries */
        /* does not change if cache enabled/disabled */
        movec.l %cacr, %d1
        or.w #(CACR_CI + CACR_CD), %d1
        movec.l %d1, %cacr
        nop
        jsr %a5@                                        /* ... off we go! */
        /* feels unlikely we're coming back, but ... */
stopped:
        stop #0x2700                    /* all done */
        br.s stopped                    /* loop on NMI */
copyend:
        .end
