#ifndef __GOGOBOOT_DISK_DOT_H__
#define __GOGOBOOT_DISK_DOT_H__

#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>

/* target platform defines this (opaque) type */
typedef struct disk_controller_t disk_controller_t;

/* target platform provides these methods */
void ide_controller_reset(disk_controller_t *ctrl);
void ide_set_register(disk_controller_t *ctrl, int reg, uint8_t val);
uint8_t ide_get_register(disk_controller_t *ctrl, int reg);
void ide_transfer_sector_write(disk_controller_t *ctrl, const void *buff);
void ide_transfer_sector_read(disk_controller_t *ctrl, void *buff);

/* common ide code provides this type */
typedef struct disk_t {
    disk_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sectors;       /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
} disk_t;

/* common ide code provides these methods */
void disk_init(void);
disk_t *disk_get_info(int nr);
int disk_get_count(void);
bool disk_data_read(int disk, void *buff, uint32_t sector, int sector_count);
bool disk_data_write(int disk, const void *buff, uint32_t sector, int sector_count);
void disk_controller_startup(disk_controller_t *ctrl);

#endif
