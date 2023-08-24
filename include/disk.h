#ifndef __GOGOBOOT_DISK_DOT_H__
#define __GOGOBOOT_DISK_DOT_H__

#include <stdbool.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>

typedef struct disk_controller_t disk_controller_t;

typedef struct disk_t {
    disk_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sectors;       /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
    /* could store name and model? */
    /* could parse full partition table? */
} disk_t;

void disk_init(void);
disk_t *disk_get_info(int nr);
int disk_get_count(void);
bool disk_data_read(int disk, void *buff, uint32_t sector, int sector_count);
bool disk_data_write(int disk, const void *buff, uint32_t sector, int sector_count);

#endif
