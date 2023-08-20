/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <stdlib.h>
#include <types.h>
#include <cpu.h>
#include <timers.h>
#include <kiss/ecb.h>
#include <kiss/hw.h>

#define MCTL_NORMAL 0x02                 /* 8-bit bus, fixed interrupt priority mode */
#define MCTL_NOCOUT (MCTL_NORMAL | 0x40) /* COUTD=1: override internal interrupt sampling oscillator */
#define MCTL_FROZEN (MCTL_NORMAL | 0x80) /* CFRZ=1: Counters frozen for read-out */
#define COUNT_PER_TICK (NS32202_CLK_INPUT / TIMER_HZ)
#define NS32202_USABLE_IRQS (0x12FF)            /* bitmask of usable external IRQ lines on MF/PIC */

volatile uint32_t timer_ticks;

timer_t gogoboot_read_timer(void)
{
    return timer_ticks; /* it's a single aligned long word, so this should be atomic? */
}

static inline uint8_t ns32202_read_reg_byte(uint8_t reg)
{
    return ecb_read_byte(KISS68030_MFPIC_NS32202 + (reg << 8));
}

static inline void ns32202_write_reg_byte(uint8_t reg, uint8_t val)
{
    ecb_write_byte_pause(KISS68030_MFPIC_NS32202 + (reg << 8), val);
}

static inline void ns32202_write_reg_word(uint8_t reg, unsigned short val)
{
    ecb_write_byte(KISS68030_MFPIC_NS32202 + (reg << 8), val);
    ecb_write_byte_pause(KISS68030_MFPIC_NS32202 + ((reg + 1) << 8), val >> 8);
}

static inline unsigned short ns32202_read_reg_word(uint8_t reg)
{
    return (ns32202_read_reg_byte(reg+1) << 8) | (ns32202_read_reg_byte(reg) & 0xFF);
}

static void ns32202_test(uint16_t high, uint16_t low)
{
    uint16_t h, l;
    ns32202_write_reg_word(NS32202_HCSV, high);
    ns32202_write_reg_word(NS32202_LCSV, low);
    h = ns32202_read_reg_word(NS32202_HCSV);
    l = ns32202_read_reg_word(NS32202_LCSV);
    if(h == high &&  l == low)
        return;
    printf("ns32202: read back from registers failed: HCSV=%04x LCSV=%04x (expected %04x, %04x)\n", h, l, high, low);
    halt();
}

void kiss_setup_interrupts(void)
{
    ns32202_write_reg_byte(NS32202_CCTL, 0x40);   /* counter control: no prescaling, counters off */
    ns32202_write_reg_byte(NS32202_MCTL, MCTL_NORMAL); /* datasheet recommends setting COUTD=1 at start of setup */

    ns32202_test(0x1234, 0x55aa);
    ns32202_test(0xaa55, 0x4321);
    
    /* Continue with initialisation (per datasheet recommended sequence) */
    ns32202_write_reg_byte(NS32202_MCTL, MCTL_NOCOUT); /* datasheet recommends setting COUTD=1 here */
    ns32202_write_reg_byte(NS32202_CCTL, 0x40);   /* counter control: no prescaling, counters off */
    ns32202_write_reg_word(NS32202_LCSV, 0xFFFF); /* program low counter for full-scale */
    ns32202_write_reg_word(NS32202_HCSV, COUNT_PER_TICK - 1); /* program high counter for HZ */
    ns32202_write_reg_byte(NS32202_CIPTR, KISS68030_TIMERL_IRQ | (KISS68030_TIMERH_IRQ << 4));  /* assign counter irqs */
    ns32202_write_reg_word(NS32202_HCCV, 0);      /* reset counter to zero */
    ns32202_write_reg_word(NS32202_LCCV, 0);      /* reset counter to zero */
    ns32202_write_reg_byte(NS32202_CICTL, 0x31);  /* clear errors, enable only high counter IRQ */
    ns32202_write_reg_byte(NS32202_IPS, 0xFF);    /* I/O port select: 0=I/O, 1=interrupt */
    ns32202_write_reg_byte(NS32202_PDIR, 0xFF);   /* Port direction: 0=Output, 1=Input */
    ns32202_write_reg_byte(NS32202_OCASN, 0x00);  /* Output clock assignment: All off */
    ns32202_write_reg_word(NS32202_ISRV, 0x0000); /* Clear all in-service bits */
    ns32202_write_reg_word(NS32202_CSRC, 0x0000); /* Interrupt cascade: Disabled on all inputs */
    ns32202_write_reg_byte(NS32202_SVCT, 0x00);   /* Software vector assignment */
    ns32202_write_reg_word(NS32202_IMSK, 0xFFFF); /* mask all ints - we now unmask when IRQ is requested */
    ns32202_write_reg_byte(NS32202_MCTL, MCTL_NORMAL); /* set mode control, bits: 8 bit bus, fixed priority mode */
    ns32202_write_reg_word(NS32202_ISRV, 0x0000); /* Clear all in-service bits */
    ns32202_write_reg_word(NS32202_CSRC, 0x0000); /* Interrupt cascade: Disabled on all inputs */
    ns32202_write_reg_word(NS32202_ISRV, 0);      /* Clear ISRV */
    ns32202_write_reg_byte(NS32202_FPRT, KISS68030_UART_IRQ);      /* Set highest priority */
    /* unused IRQ lines are tied high; mark them as active low */
    /* used IRQ lines are active high (the bus lines go through an inverter) */
    ns32202_write_reg_word(NS32202_TPL, NS32202_USABLE_IRQS); /* Polarity: 0=Falling/Low, 1=Rising/High (Edge/Level) */
    /* errata says to program ELTG after TPL */
    ns32202_write_reg_word(NS32202_ELTG, 0x0000); /* Edge/Level: 0=Edge, 1=Level */
    ns32202_write_reg_byte(NS32202_MCTL, MCTL_NORMAL); /* Enable internal interrupt sampling clock */
    /* Enable the high counter to act as our timer tick */
    ns32202_write_reg_byte(NS32202_CCTL, 0x48);   /* counter control: enable high counter only */

    /* set MF/PIC config register to 16 interrupt mode, vector base 0x40, shift 00.
     * Top four vector bits are set in the config register, low four bits are from
     * the NS32202 so the vector range is 0x40 -- 0x4F, ie VEC_USER -- VEC_USER+16 */
    ecb_write_byte_pause(KISS68030_MFPIC_CFGREG, 0x44);

    /* unmask our timer interupt */
    ns32202_write_reg_word(NS32202_IMSK, ns32202_read_reg_word(NS32202_IMSK) & ~(1 << KISS68030_TIMERH_IRQ));
    printf("imsk=%x\n", ns32202_read_reg_word(NS32202_IMSK));

    cpu_interrupts_on();
}
