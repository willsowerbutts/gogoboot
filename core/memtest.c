/* 2023-09-21 William R Sowerbutts - modified for integration into gogoboot
 * taken from Amiga Test Kit -- https://github.com/keirf/amiga-stuff/
 */
/*
 * memory.c
 * 
 * Detect and test available memory.
 * 
 * Written & released by Keir Fraser <keir.xen@gmail.com>
 * 
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#include <stdlib.h>
#include <uart.h>
#include <init.h>
#include <cpu.h>
#include <net.h>

/* sprintf helpers */
#define mb_whole(x) ((x) >> 20)
#define mb_frac(x) (mul32((x)>>4, 100) >> 16)

enum {
    CACHE_FULL,   // cpu_cache_enable
    CACHE_NODATA, // cpu_cache_enable_nodata
    CACHE_NONE,   // cpu_cache_disable
    CACHE_INIT
};

/* Testing is split into rounds and subrounds. Each subround covers all 
 * memory before proceeding to the next. Even subrounds fill memory; 
 * odd subrounds check the preceding fill. */
static unsigned int test_round, test_subround;
/* Memory range being tested within the current subround. */
static uint32_t test_start, test_end;
static uint32_t error_counter;
/* LFSR seed for pseudo-random fill. This gets saved at the start of fill 
 * so that the same sequence can be reproduced and checked. */
static uint32_t test_seed, test_seed_saved;
static int test_cache_mode;
static int spinner_frame;

const char spinner_symbol[4] = "\\|/-";

/* 32-bit sequence of length 2^32-1 (all 32-bit values except zero). */
static inline __attribute__((always_inline)) uint32_t lfsr(uint32_t x)
{
    asm volatile (
        "lsr.l #1,%0; bcc.s 1f; eor.l #0x80000062,%0; 1:"
        : "=d" (x) : "0" (x));
    return x;
}

/* Fill every 32-bit word from @start to @end. */
static void fill_32(
    uint32_t fill, volatile uint32_t *start, volatile uint32_t *end)
{
    uint32_t x, y;
    asm volatile (
        "1: move.l %2,(%3)+; move.l %2,(%3)+; "
        "move.l %2,(%3)+; move.l %2,(%3)+; dbf %4,1b"
        : "=a" (x), "=d" (y)
        : "d" (fill), "0" (start), "1" ((end-start)/4-1));
}

/* Fill every other 16-bit word from @start to @end. */
static void fill_alt_16(
    uint16_t fill, uint16_t shift,
    volatile uint32_t *start, volatile uint32_t *end)
{
    uint32_t x = (uint32_t)start + shift, y;
    asm volatile (
        "1: move.w %2,(%3); move.w %2,4(%3); move.w %2,8(%3); "
        "move.w %2,12(%3); lea 16(%3),%3; dbf %4,1b"
        : "=a" (x), "=d" (y)
        : "d" (fill), "0" (x), "1" ((end-start)/4-1));
}

static uint16_t check_pattern(
    uint32_t check, volatile uint32_t *start, volatile uint32_t *end)
{
    uint32_t x, y, z, val;
    asm volatile (
        "1: move.l (%5)+,%2; eor.l %4,%2; or.l %2,%3; "
        "move.l (%5)+,%2; eor.l %4,%2; or.l %2,%3; "
        "move.l (%5)+,%2; eor.l %4,%2; or.l %2,%3; "
        "move.l (%5)+,%2; eor.l %4,%2; or.l %2,%3; "
        "dbf %6,1b; move.w %3,%2; swap %3; or.w %2,%3"
        : "=a" (x), "=d" (y), "=&d" (z), "=d" (val)
        : "d" (check), "0" (start), "1" ((end-start)/4-1), "3" (0));
    return (uint16_t)val;
}

static int test_memory_range(void)
{
    volatile uint32_t *p;
    volatile uint32_t *start = (volatile uint32_t *)((test_start+15) & ~15);
    volatile uint32_t *end = (volatile uint32_t *)(test_end & ~15);
    uint32_t a = 0, x;

    if (start >= end)
        return 0;

    putchar(spinner_symbol[spinner_frame]);
    spinner_frame = (spinner_frame + 1) % 4;

    switch (test_subround) {

    case 0:
        /* Random numbers. */
        x = test_seed;
        for (p = start; p != end;) {
            *p++ = x = lfsr(x);
            *p++ = x = lfsr(x);
            *p++ = x = lfsr(x);
            *p++ = x = lfsr(x);
        }
        test_seed = x;
        break;
    case 1:
        x = test_seed;
        for (p = start; p != end;) {
            a |= *p++ ^ (x = lfsr(x));
            a |= *p++ ^ (x = lfsr(x));
            a |= *p++ ^ (x = lfsr(x));
            a |= *p++ ^ (x = lfsr(x));
        }
        test_seed = x;
        break;

    case 2:
        /* Start with all 0s. Write 1s to even words. */
        fill_32(0, start, end);
        fill_alt_16(~0, 0, start, end);
        break;
    case 3:
        a |= check_pattern(0xffff0000, start, end);
        break;

    case 4:
        /* Start with all 0s. Write 1s to odd words. */
        fill_32(0, start, end);
        fill_alt_16(~0, 2, start, end);
        break;
    case 5:
        a |= check_pattern(0x0000ffff, start, end);
        break;

    case 6:
        /* Start with all 1s. Write 0s to even words. */
        fill_32(~0, start, end);
        fill_alt_16(0, 0, start, end);
        break;
    case 7:
        a |= check_pattern(0x0000ffff, start, end);
        break;

    case 8:
        /* Start with all 1s. Write 0s to odd words. */
        fill_32(~0, start, end);
        fill_alt_16(0, 2, start, end);
        break;
    case 9:
        a |= check_pattern(0xffff0000, start, end);
        break;
    }

    putchar('\r');

    /* Errors found: print diagnostic */
    if (a != 0) {
        error_counter++;
        printf("Errors found in memory range 0x%08lx-0x%08lx\n", (uint32_t)start, (uint32_t)end-1);
        printf("Error bits: D31..D24  D23..D16  D15...D8  D7....D0\n");
        printf("(X=error)   76543210  76543210  76543210  76543210\n");
        printf("            %c%c%c%c%c%c%c%c  %c%c%c%c%c%c%c%c  "
                           "%c%c%c%c%c%c%c%c  %c%c%c%c%c%c%c%c\n",
                (a & (1u<<31)) ? 'X' : '-', (a & (1u<<30)) ? 'X' : '-',
                (a & (1u<<29)) ? 'X' : '-', (a & (1u<<28)) ? 'X' : '-',
                (a & (1u<<27)) ? 'X' : '-', (a & (1u<<26)) ? 'X' : '-',
                (a & (1u<<25)) ? 'X' : '-', (a & (1u<<24)) ? 'X' : '-',
                (a & (1u<<23)) ? 'X' : '-', (a & (1u<<22)) ? 'X' : '-',
                (a & (1u<<21)) ? 'X' : '-', (a & (1u<<20)) ? 'X' : '-',
                (a & (1u<<19)) ? 'X' : '-', (a & (1u<<18)) ? 'X' : '-',
                (a & (1u<<17)) ? 'X' : '-', (a & (1u<<16)) ? 'X' : '-',
                (a & (1u<<15)) ? 'X' : '-', (a & (1u<<14)) ? 'X' : '-',
                (a & (1u<<13)) ? 'X' : '-', (a & (1u<<12)) ? 'X' : '-',
                (a & (1u<<11)) ? 'X' : '-', (a & (1u<<10)) ? 'X' : '-',
                (a & (1u<< 9)) ? 'X' : '-', (a & (1u<< 8)) ? 'X' : '-',
                (a & (1u<< 7)) ? 'X' : '-', (a & (1u<< 6)) ? 'X' : '-',
                (a & (1u<< 5)) ? 'X' : '-', (a & (1u<< 4)) ? 'X' : '-',
                (a & (1u<< 3)) ? 'X' : '-', (a & (1u<< 2)) ? 'X' : '-',
                (a & (1u<< 1)) ? 'X' : '-', (a & (1u<< 0)) ? 'X' : '-');
    }

    return 0;
}

static void print_memory_test_type(void)
{
    switch (test_subround) {
    case 0:
        printf("Round %u.%u: Random Fill (seed=0x%08lx)", 
                test_round+1, 1, test_seed);
        /* Save the seed we use for this fill. */
        test_seed_saved = test_seed;
        break;
    case 1:
        /* Restore seed we used for fill. */
        test_seed = test_seed_saved;
        break;
    case 2: case 4: case 6: case 8:
        printf("Round %u.%u: Checkboard #%u",
                test_round+1, test_subround/2+1, test_subround/2);
        break;
    }
    if((test_subround & 1) == 0){
        if(error_counter)
            printf(" (ERROR COUNTER: %ld)\n", error_counter);
        else
            printf("\n");
    }
}

static void memory_test_next_cpu_cache_mode(void)
{
#ifdef CPU_68020_OR_LATER
    switch(test_cache_mode){
        case CACHE_FULL:
            test_cache_mode = CACHE_NODATA;
            cpu_cache_enable_nodata();
            printf("CPU cache disabled\n");
            break;
        case CACHE_NODATA:
            test_cache_mode = CACHE_NONE;
            cpu_cache_disable();
            printf("CPU cache enabled (instructions only)\n");
            break;
        case CACHE_NONE:
        case CACHE_INIT:
            test_cache_mode = CACHE_FULL;
            cpu_cache_enable();
            printf("CPU cache enabled (data and instructions)\n");
            break;
    }
#endif
}

static void init_memory_test(void)
{
    test_round = test_subround = 0;
    test_seed = 0x12341234;
    error_counter = 0;
    test_cache_mode = CACHE_INIT;
    spinner_frame = 0;
    memory_test_next_cpu_cache_mode();
    print_memory_test_type();
}

static void memory_test_next_subround(void)
{
    if (test_subround++ == 9) {
        test_subround = 0;
        test_round++;
        memory_test_next_cpu_cache_mode();
    }
    print_memory_test_type();
}

/* original amiga test kit uses chunks no larger than 512kB */
/* this is useful to narrow down where memory errors exist */
#define TEST_CHUNK_SIZE 0x80000

void memory_test(uint32_t base, uint32_t size)
{
    bool done = false;
    const char *addr_err;
    uint32_t test_remain, test_size, chunk_end;

    if(size == 0)
        return;

    printf("memory test: testing 0x%08lx -- 0x%08lx (Press Q to end)\n", base, base+size-1);

    addr_err = check_writable_range(base, size, false);
    if(addr_err){
        printf("memory test: address range error: %s\n", addr_err);
        return;
    }

    init_memory_test();

    while(!done){
        test_start = base;
        test_remain = size;

        while(test_remain && !done){ 
            if(test_remain > TEST_CHUNK_SIZE)
                test_size = TEST_CHUNK_SIZE;
            else
                test_size = test_remain;

            /* test_memory_range() expects the end bound to be +1 */
            test_end = test_start + test_size;

            /* we want to start the next block on a chunk boundary */
            chunk_end = test_end & ~(TEST_CHUNK_SIZE-1);
            if(chunk_end != test_end && chunk_end > test_start){
                test_end = chunk_end;
                test_size = test_end - test_start;
            }

            /* keep the network alive */
            net_pump();

            /* check if the user is bored of this yet */
            if(uart_check_cancel_key())
                done = true;
            else
                test_memory_range();

            test_remain -= test_size;
            test_start += test_size;
        }

        if(!done)
            memory_test_next_subround();
    }

    cpu_cache_enable();
    printf("\nmemory test: ended\n");
}
