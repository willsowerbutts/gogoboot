#ifndef __GOGOBOOT_Q40_IDE_DOT_H__
#define __GOGOBOOT_Q40_IDE_DOT_H__

struct disk_controller_t
{
    uint16_t base_io;
    volatile uint16_t *data_reg;
};

#endif
