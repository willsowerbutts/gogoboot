
#ifndef __Q40IDE_DOT_H__
#define __Q40IDE_DOT_H__

#include <stdbool.h>
#include <types.h>
#include "ff.h"
#include "diskio.h"

typedef struct ide_controller_t {
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
} ide_controller_t;

typedef struct ide_disk_t {
    ide_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sectors;       /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
    /* name and model? */
    /* partition table? */
} ide_disk_t;

void q40_ide_init(void);
ide_disk_t *q40_get_disk_info(int nr);
int q40_ide_get_disk_count(void);
bool q40_ide_read(int disk, void *buff, uint32_t sector, int sector_count);
bool q40_ide_write(int disk, const void *buff, uint32_t sector, int sector_count);

#endif
