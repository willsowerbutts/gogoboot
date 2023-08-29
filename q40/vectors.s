        .globl  vector_table
        .globl  timer_ticks
        .globl  uart_write_str
        .globl  uart_write_byte
        .globl  report_exception
        .globl  rom_pointer

        .section .text
        .align 4

/* 68040 exception vector table */
vector_table:
        .long   unhandled_exception  /* 0 initial ISP */
        .long   unhandled_exception  /* 1 initial PC */
        .long   unhandled_exception  /* 2 access fault / bus error */
        .long   unhandled_exception  /* 3 address error */
        .long   unhandled_exception  /* 4 illegal instruction */
        .long   unhandled_exception  /* 5 integer divide by zero */
        .long   unhandled_exception  /* 6 CHK, CHK2 instruction */
        .long   unhandled_exception  /* 7 FTRAPcc, TRAPcc, TRAPV instruction */
        .long   unhandled_exception  /* 8 privilege violation */
        .long   unhandled_exception  /* 9 trace */
        .long   unhandled_exception  /* 10 line 1010 emulator */
        .long   unhandled_exception  /* 11 line 1111 emulator */
        .long   unhandled_exception  /* 12 (reserved) */
        .long   unhandled_exception  /* 13 defined for 020/030 but not 040 */
        .long   unhandled_exception  /* 14 format error */
        .long   unhandled_exception  /* 15 uninitialised interrupt */
        .long   unhandled_exception  /* 16 (reserved) */
        .long   unhandled_exception  /* 17 (reserved) */
        .long   unhandled_exception  /* 18 (reserved) */
        .long   unhandled_exception  /* 19 (reserved) */
        .long   unhandled_exception  /* 20 (reserved) */
        .long   unhandled_exception  /* 21 (reserved) */
        .long   unhandled_exception  /* 22 (reserved) */
        .long   unhandled_exception  /* 23 (reserved) */
        .long   unhandled_exception  /* 24 spurious interrupt */
        .long   interrupt_level_1    /* 25 level 1 interrupt autovector */
        .long   interrupt_level_2    /* 26 level 2 interrupt autovector */
        .long   interrupt_level_3    /* 27 level 3 interrupt autovector */
        .long   interrupt_level_4    /* 28 level 4 interrupt autovector */
        .long   interrupt_level_5    /* 29 level 5 interrupt autovector */
        .long   interrupt_level_6    /* 30 level 6 interrupt autovector */
        .long   interrupt_level_7    /* 31 level 7 interrupt autovector */
        .long   trap_0               /* 32 trap 0 instruction */
        .long   unhandled_exception  /* 33 trap 1 instruction */
        .long   unhandled_exception  /* 34 trap 2 instruction */
        .long   unhandled_exception  /* 35 trap 3 instruction */
        .long   unhandled_exception  /* 36 trap 4 instruction */
        .long   unhandled_exception  /* 37 trap 5 instruction */
        .long   unhandled_exception  /* 38 trap 6 instruction */
        .long   unhandled_exception  /* 39 trap 7 instruction */
        .long   unhandled_exception  /* 40 trap 8 instruction */
        .long   unhandled_exception  /* 41 trap 9 instruction */
        .long   unhandled_exception  /* 42 trap 10 instruction */
        .long   unhandled_exception  /* 43 trap 11 instruction */
        .long   unhandled_exception  /* 44 trap 12 instruction */
        .long   unhandled_exception  /* 45 trap 13 instruction */
        .long   unhandled_exception  /* 46 trap 14 instruction */
        .long   unhandled_exception  /* 47 trap 15 instruction */

trap_0: /* SMSQ/E and QDOS use this vector for "enter supervisor mode";
           we're already in supervisor mode, so not much to do here! */
        rte

/* Q40 interrupt handler */
interrupt_level_2:
        move.l %d0, -(%sp)
        moveb 0xff000000, %d0           /* load interrupt status */
        movew %d0, %ccr                 /* test bit 3 */
        bpls interrupt_level_2_done     /* branch if bit 3 = 0 */
        /* bit 3 set: frame interrupt (50/200Hz timer tick) */
        st.b 0xff000024                 /* frame interrupt ack/clear */
        addq.l #1,(timer_ticks) 
interrupt_level_2_done:
        move.l (%sp)+, %d0
        rte

/* handlers for things that might happen and which are, generally, bad */

interrupt_level_1:
        pea '1'
        bra bad_interrupt

interrupt_level_3:
        pea '3'
        bra bad_interrupt

interrupt_level_4:
        pea '4'
        bra bad_interrupt

interrupt_level_5:
        pea '5'
        bra bad_interrupt

interrupt_level_6:
        pea '6'
        bra bad_interrupt

interrupt_level_7:
        pea '7'
        bra bad_interrupt

/* for bad interrupts we just report the interupt number and halt */
bad_interrupt:
        pea bad_interrupt_message
        jsr uart_write_str
        move.l (%sp)+, %d0
        jsr uart_write_byte
        bra halt

/* for exceptions we call a routine that reports the machine state before halting */
unhandled_exception:
        pea (%sp)
        jsr report_exception
halt:   stop #2701
        br.s halt

        .section .rodata
bad_interrupt_message:
        .ascii ":( UNEXPECTED INT \0"
rom_pointer:
        .long   0                       /* I prefer this to using -Wno-nonnull */

        .end
