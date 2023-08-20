#ifndef __GOGOBOOT_Q40_IDE_DOT_H__
#define __GOGOBOOT_Q40_IDE_DOT_H__

struct disk_controller_t
{
    uint16_t base_io;
    volatile uint16_t *data_reg;
    volatile uint8_t *ctl_reg;
    volatile uint8_t *error_reg;
    volatile uint8_t *feature_reg;
    volatile uint8_t *nsect_reg;
    volatile uint8_t *lbal_reg;
    volatile uint8_t *lbam_reg;
    volatile uint8_t *lbah_reg;
    volatile uint8_t *device_reg;
    volatile uint8_t *status_reg;
    volatile uint8_t *command_reg;
    volatile uint8_t *altstatus_reg;
};

#endif
