#ifndef __Q40ISA_DOT_H__
#define __Q40ISA_DOT_H__

#include <types.h>

#define ISA_BASE_ADDR 0xff400000
#define ISA_XLATE_ADDR_BYTE(addr) ((volatile uint8_t *)(ISA_BASE_ADDR+1+((addr)<<2)))
#define ISA_XLATE_ADDR_WORD(addr) ((volatile uint16_t*)(ISA_BASE_ADDR + ((addr)<<2)))

static inline uint16_t isa_read_word(uint16_t addr)
{
    return *ISA_XLATE_ADDR_WORD(addr);
}

static inline void isa_write_word(uint16_t addr, uint16_t val)
{
    *ISA_XLATE_ADDR_WORD(addr) = val;
}

static inline uint8_t isa_read_byte(uint16_t addr)
{
    return *ISA_XLATE_ADDR_BYTE(addr);
}

static inline void isa_write_byte(uint16_t addr, uint8_t val)
{
    *ISA_XLATE_ADDR_BYTE(addr) = val;
}

static inline void isa_write_byte_pause(uint16_t addr, uint8_t val)
{
    *ISA_XLATE_ADDR_BYTE(addr) = val;
    *ISA_XLATE_ADDR_BYTE(0x80) = 0xff;
}

static inline void isa_slow_down(void)
{
    isa_write_byte(0x80, 0xff);
}

#endif
