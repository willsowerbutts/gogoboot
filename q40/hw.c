/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <types.h>
#include <q40/isa.h>
#include <q40/hw.h>
#include <init.h>
#include <cpu.h>

volatile uint32_t timer_ticks;

uint32_t mem_get_max_possible(void)
{
    /* code will need adjusting to support 128MB option boards */
    /* I do not have one of these to test with :( */
    return MAX_RAM_SIZE << 20;
}

uint32_t mem_get_granularity(void)
{
    return 1024*1024;
}

uint32_t mem_get_rom_below_addr(void)
{
    return 96*1024;
}

void early_init(void)
{
    q40_isa_reset();
    q40_led(true);
}

void target_hardware_init(void)
{
    printf("Initialise video: ");
    q40_graphics_init(3);
    printf("done\n");
}

timer_t gogoboot_read_timer(void)
{
    return timer_ticks; /* it's a single aligned long word, so this should be atomic? */
}

void setup_interrupts(void)
{
    /* configure MASTER chip */
    *q40_keyboard_interrupt_enable = 0;
    *q40_isa_interrupt_enable = 0;
    *q40_sample_interrupt_enable = 0;
    *q40_sample_rate = 0;
#if TIMER_HZ == 200
    *q40_frame_rate = 1;
#elif TIMER_HZ == 50
    *q40_frame_rate = 0;
#else
    #error Unsupported TIMER_HZ value (use 50 or 200)
#endif
    *q40_keyboard_interrupt_ack = 0xff;
    *q40_frame_interrupt_ack = 0xff;
    *q40_sample_interrupt_ack = 0xff;
    cpu_interrupts_on();
}

static void q40_delay(uint32_t count)
{
    uint32_t x;
    while(count--)
        x += *q40_interrupt_status;
}

void q40_isa_reset(void)
{
    *q40_isa_bus_reset = 0xff;
    /* assume we don't have timer interrupts yet */
    q40_delay(100000);
    *q40_isa_bus_reset = 0;
}

void q40_led(bool on)
{
    *q40_led_control = on ? 0xff : 0;
}

void q40_graphics_init(int mode)
{
    // behold my field of modes, for in it there grow but four
    mode &= 3;

    // program the display controller
    *q40_display_control = mode;

    // clear entire video memory (1MB)
    memset((void*)VIDEO_RAM_BASE, 0xaa, 1024*1024);

    // the CPU caches video memory so we need to force it to write the updates out
    // otherwise we get weird artefacts on the screen that hang around forever!
    cpu_cache_flush();
}

void target_mem_init(void)
{
    rom_below_addr = 0;
    stack_base = ram_size - DEFAULT_STACK_SIZE;
    stack_size = DEFAULT_STACK_SIZE;
    stack_top = stack_base + stack_size;
    /* attempts to load_data() into addresses below bounce_below_addr 
     * will result in the bounce buffer being employed */
    bounce_below_addr = (((uint32_t)&bss_end) + 3) & ~3; /* round to longword */

    heap_size = ram_size / 4;  /* not more than 25% of RAM */
    if(heap_size > MAXHEAP)    /* and not too much */
        heap_size = MAXHEAP;

    heap_base = ram_size - heap_size;
    heap_size -= stack_size;
}
