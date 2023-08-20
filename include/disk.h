#ifndef __GOGOBOOT_IDE_DOT_H__
#define __GOGOBOOT_IDE_DOT_H__

#include <stdbool.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>

typedef struct disk_controller_t {
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
} disk_controller_t;

typedef struct disk_t {
    disk_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sectors;       /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
    /* could store name and model? */
    /* could parse full partition table? */
} disk_t;

void gogoboot_disk_init(void);
disk_t *gogoboot_get_disk_info(int nr);
int gogoboot_disk_get_disk_count(void);
bool gogoboot_disk_read(int disk, void *buff, uint32_t sector, int sector_count);
bool gogoboot_disk_write(int disk, const void *buff, uint32_t sector, int sector_count);

#endif
