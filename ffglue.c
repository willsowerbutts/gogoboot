/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#ifdef TARGET_Q40
#include <q40/hw.h>
#endif

/* glue for FatFs library */

DSTATUS disk_status(BYTE pdrv)
{
    disk_t *disk_disk = disk_get_info(pdrv);

    if(!disk_disk)
        return STA_NOINIT;

    return disk_disk->fat_fs_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    disk_t *disk_disk = disk_get_info(pdrv);

    if(!disk_disk)
        return STA_NOINIT;

    disk_disk->fat_fs_status = 0;
    return disk_disk->fat_fs_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    disk_t *disk_disk = disk_get_info(pdrv);

    if(!disk_disk)
        return RES_PARERR;

    if(disk_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    if(disk_data_read(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    disk_t *disk_disk = disk_get_info(pdrv);

    if(!disk_disk)
        return RES_PARERR;

    if(disk_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    if(disk_disk->fat_fs_status & STA_PROTECT)
        return RES_WRPRT;

    if(disk_data_write(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    disk_t *disk_disk = disk_get_info(pdrv);

    if(!disk_disk)
        return RES_PARERR;

    if(disk_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    switch(cmd){
        case CTRL_SYNC:
        case CTRL_TRIM:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *((long*)buff) = 512;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *((long*)buff) = disk_disk->sectors;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

void* ff_memalloc (UINT msize)
{
    return malloc(msize);
}

void ff_memfree (void* mblock)
{
    free(mblock);
}
