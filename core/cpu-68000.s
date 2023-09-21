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
cpu_cache_invalidate:           /* flush data and instruction caches */
cpu_cache_enable_nodata:        /* enable cpu instruction cache, disable data cache */
cpu_cache_enable:               /* enable both cpu caches */
cpu_cache_disable:              /* disable and flush data and intsruction caches */
        rts

cpu_interrupts_on:              /* enable CPU interrupts */
        and.w #0xf8ff, %sr
        rts

cpu_interrupts_off:             /* disable CPU interrupts */
        or.w #0x0700, %sr
        rts

        .end
