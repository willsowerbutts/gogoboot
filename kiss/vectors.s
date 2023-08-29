        .include "kiss/kisshw.s"

        .globl  vector_table
        .globl  uart_write_str
        .globl  uart_write_byte
        .globl  report_exception
        .globl  timer_ticks
        .globl  halt

        .section .text
        .align 4

/* 68030 exception vector table */
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
        .long   unhandled_exception  /* 13 coprocessor protocol violation */
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
        .long   unhandled_exception  /* 32 trap 0 instruction */
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
        .long   unhandled_exception  /* 48 FPCP exception */
        .long   unhandled_exception  /* 49 FPCP exception */
        .long   unhandled_exception  /* 50 FPCP exception */
        .long   unhandled_exception  /* 51 FPCP exception */
        .long   unhandled_exception  /* 52 FPCP exception */
        .long   unhandled_exception  /* 53 FPCP exception */
        .long   unhandled_exception  /* 54 FPCP exception */
        .long   unhandled_exception  /* 55 reserved */
        .long   unhandled_exception  /* 56 MMU exception */
        .long   unhandled_exception  /* 57 MMU exception */
        .long   unhandled_exception  /* 58 MMU exception */
        .long   unhandled_exception  /* 59 reserved */
        .long   unhandled_exception  /* 60 reserved */
        .long   unhandled_exception  /* 61 reserved */
        .long   unhandled_exception  /* 62 reserved */
        .long   unhandled_exception  /* 63 reserved */
        .long   ns202_irq_0          /* 64 user vector 0 (NS32202 IRQ) */
        .long   ns202_irq_1          /* 65 user vector 1 (NS32202 IRQ) */
        .long   ns202_irq_2          /* 66 user vector 2 (NS32202 IRQ) */
        .long   ns202_irq_3          /* 67 user vector 3 (NS32202 IRQ) */
        .long   ns202_irq_4          /* 68 user vector 4 (NS32202 IRQ) */
        .long   ns202_irq_5          /* 69 user vector 5 (NS32202 IRQ) */
        .long   ns202_irq_6          /* 70 user vector 6 (NS32202 IRQ) */
        .long   ns202_irq_7          /* 71 user vector 7 (NS32202 IRQ) */
        .long   ns202_irq_8          /* 72 user vector 8 (NS32202 IRQ) */
        .long   ns202_irq_9          /* 73 user vector 9 (NS32202 IRQ) */
        .long   ns202_irq_a          /* 74 user vector 10 (NS32202 IRQ) */
        .long   ns202_irq_b          /* 75 user vector 11 (NS32202 IRQ) */
        .long   ns202_irq_c          /* 76 user vector 12 (NS32202 IRQ) */
        .long   ns202_irq_d          /* 77 user vector 13 (NS32202 IRQ) */
        .long   ns202_irq_e          /* 78 user vector 14 (NS32202 IRQ) */
        .long   ns202_irq_f          /* 79 user vector 15 (NS32202 IRQ) */

/* handlers for things that might happen and which are, generally, bad */

interrupt_level_1:
        pea '1'
        bra bad_interrupt

interrupt_level_2:
        pea '2'
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

ns202_irq_0:
        pea '0'
        bra bad_ns202_irq

ns202_irq_1:
        pea '1'
        bra bad_ns202_irq

ns202_irq_2:
        pea '2'
        bra bad_ns202_irq

ns202_irq_3:
        pea '3'
        bra bad_ns202_irq

ns202_irq_4:
        pea '4'
        bra bad_ns202_irq

ns202_irq_5:
        pea '5'
        bra bad_ns202_irq

ns202_irq_6:
        pea '6'
        bra bad_ns202_irq

ns202_irq_7:
        pea '7'
        bra bad_ns202_irq

ns202_irq_8:
        pea '8'
        bra bad_ns202_irq

ns202_irq_9:
        pea '9'
        bra bad_ns202_irq

ns202_irq_a:
        pea 'A'
        bra bad_ns202_irq

ns202_irq_b:
        pea 'B'
        bra bad_ns202_irq

ns202_irq_c:
        pea 'C'
        bra bad_ns202_irq

ns202_irq_d:
        /* this is our timer interrupt */
        move.l %d0, -(%sp)
        /* generate an end of interrupt cycle for NS32202 */
        move.b (KISS68030_ECBIO_BASE + KISS68030_MFPIC_ADDR + (NS32202_EOI << 8)), %d0
        /* increment timer tick counter */
        addq.l #1,(timer_ticks) 
        move.l (%sp)+, %d0
        rte

ns202_irq_e:
        pea 'E'
        bra bad_ns202_irq

ns202_irq_f:
        pea 'F'
        bra bad_ns202_irq

/* for bad interrupts we just report the interupt number and halt */
bad_interrupt:
        pea bad_interrupt_message
        jsr uart_write_str
        move.l (%sp)+, %d0
        jsr uart_write_byte
        bra halt

bad_ns202_irq:
        pea bad_ns202_irq_message
        jsr uart_write_str
        move.l (%sp)+, %d0
        jsr uart_write_byte
        bra halt

/* for exceptions we call a routine that reports the machine state before halting */
unhandled_exception:
        pea (%sp)
        jsr report_exception
        bra halt

        .section .rodata
bad_ns202_irq_message:
        .ascii ":( UNEXPECTED NS32202 IRQ \0"
bad_interrupt_message:
        .ascii ":( UNEXPECTED INT \0"

        .end
