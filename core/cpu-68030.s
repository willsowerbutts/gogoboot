        .include "core/cpu-68030-bits.s"
        .globl  cpu_cache_enable
        .globl  cpu_cache_enable_nodata
        .globl  cpu_cache_disable
        .globl  cpu_cache_flush
        .globl  cpu_cache_invalidate
        .globl  cpu_interrupts_on
        .globl  cpu_interrupts_off

        .section .text
        .even

cpu_cache_flush:                /* flush data cache */
        /* does not change if cache enabled/disabled */
        movec.l %cacr, %d0
        or.w #(CACR_CD), %d0
cpu_cache_write:
        movec.l %d0, %cacr
        nop
        pflusha
        nop
        rts

cpu_cache_invalidate:           /* flush data and instruction caches */
        /* does not change if cache enabled/disabled */
        movec.l %cacr, %d0
        or.w #(CACR_CI + CACR_CD), %d0
        bra.s cpu_cache_write

cpu_cache_enable_nodata:        /* enable cpu instruction cache, disable data cache */
        move.l #(CACR_EI), %d0
        movec %d0, %cacr        /* enable instruction cache only */
        rts

cpu_cache_enable:               /* enable both cpu caches */
        move.l #(CACR_EI+CACR_ED), %d0
        movec %d0, %cacr        /* enable data, instruction caches */
        rts

cpu_cache_disable:              /* disable and flush data and intsruction caches */
        move.l #(CACR_CI+CACR_CD), %d0
        movec %d0, %cacr        /* disable and clear data, instruction caches */
        pflusha
        rts

cpu_interrupts_on:              /* enable CPU interrupts */
        and.w #0xf8ff, %sr
        rts

cpu_interrupts_off:             /* disable CPU interrupts */
        or.w #0x0700, %sr
        rts

        .end
