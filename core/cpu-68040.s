        .include "core/cpu-68040-bits.s"
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
        cpusha %dc              /* write back data cache entries */
        rts

cpu_cache_invalidate:           /* flush data and instruction caches */
        cpusha %bc              /* write back and invalidate all data/instruction cache entries */
        rts

cpu_cache_enable_nodata:        /* enable cpu instruction cache, disable data cache */
        cpusha %dc              /* write back data cache entries */
        move.l #(CACR_EI), %d0
        movec %d0, %cacr        /* enable instruction cache only */
        rts

cpu_cache_enable:               /* enable both cpu caches */
        move.l #(CACR_EI+CACR_ED), %d0
        movec %d0, %cacr        /* enable data, instruction caches */
        rts

cpu_cache_disable:              /* disable and flush data and intsruction caches */
        move.l #0, %d0
        cpusha %bc              /* write back and invalidate all data/instruction cache entries */
        pflusha
        movec %d0, %cacr        /* disable data, instruction caches */
        cpusha %bc              /* write back and invalidate all data/instruction cache entries */
        nop
        rts

cpu_interrupts_on:              /* enable CPU interrupts */
        and.w #0xf8ff, %sr
        rts

cpu_interrupts_off:             /* disable CPU interrupts */
        or.w #0x0700, %sr
        rts

        .end
