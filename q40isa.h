#ifndef __Q40ISA_DOT_H__
#define __Q40ISA_DOT_H__

#define ISA_BASE_ADDR 0xff400000

static inline unsigned short isa_read_word(unsigned short addr)
{
    return *((volatile unsigned short *)(ISA_BASE_ADDR + (addr << 2)));
}

static inline void isa_write_word(unsigned short addr, unsigned short val)
{
    *((volatile unsigned short *)(ISA_BASE_ADDR + (addr << 2))) = val;
}

static inline unsigned char isa_read_byte(unsigned short addr)
{
    return *((volatile unsigned char *)(ISA_BASE_ADDR + 1 + (addr << 2)));
}

static inline void isa_write_byte(unsigned short addr, unsigned char val)
{
    *((volatile unsigned char *)(ISA_BASE_ADDR + 1 + (addr << 2))) = val;
}

#endif
