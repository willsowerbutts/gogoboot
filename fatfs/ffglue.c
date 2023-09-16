/* 
 * 2023-07-25 William R Sowerbutts
 * Interface IDE driver to FatFs by ChaN
 *   http://elm-chan.org/fsw/ff/00index_e.html 
 */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <rtc.h>

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

DWORD get_fattime (void)
{
    uint32_t result;
    /*
        bit31:25 Year origin from the 1980 (0..127, e.g. 37 for 2017)
        bit24:21 Month (1..12)
        bit20:16 Day of the month (1..31)
        bit15:11 Hour (0..23)
        bit10:5  Minute (0..59)
        bit4:0   Second / 2 (0..29, e.g. 25 for 50)
    */
    rtc_time_t now;
    rtc_read_clock(&now);
    result  = (now.year   - 1980) << 25;
    result |= (now.month        ) << 21;
    result |= (now.day          ) << 16;
    result |= (now.hour         ) << 11;  
    result |= (now.minute       ) <<  5;
    result |= (now.second       ) >>  1;
    return result;
}
