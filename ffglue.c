/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <types.h>
#include "tinyalloc.h"
#include "ff.h"
#include "diskio.h"
#include "q40hw.h"
#include "q40ide.h"

/* glue for FatFs library */

DSTATUS disk_status(BYTE pdrv)
{
    ide_disk_t *ide_disk = q40_get_disk_info(pdrv);

    if(!ide_disk)
        return STA_NOINIT;

    return ide_disk->fat_fs_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    ide_disk_t *ide_disk = q40_get_disk_info(pdrv);

    if(!ide_disk)
        return STA_NOINIT;

    ide_disk->fat_fs_status = 0;
    return ide_disk->fat_fs_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    ide_disk_t *ide_disk = q40_get_disk_info(pdrv);

    if(!ide_disk)
        return RES_PARERR;

    if(ide_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    if(q40_ide_read(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    ide_disk_t *ide_disk = q40_get_disk_info(pdrv);

    if(!ide_disk)
        return RES_PARERR;

    if(ide_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    if(ide_disk->fat_fs_status & STA_PROTECT)
        return RES_WRPRT;

    if(q40_ide_write(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    ide_disk_t *ide_disk = q40_get_disk_info(pdrv);

    if(!ide_disk)
        return RES_PARERR;

    if(ide_disk->fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;

    switch(cmd){
        case CTRL_SYNC:
        case CTRL_TRIM:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *((long*)buff) = 512;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *((long*)buff) = ide_disk->sectors;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

static int decode_bcd_digit(uint8_t val)
{
    val &= 0x0f;
    return (val > 9) ? 0 : val;
}

static int decode_bcd_byte(uint8_t val)
{
    return (decode_bcd_digit(val>>4)*10) + decode_bcd_digit(val);
}

DWORD get_fattime (void)
{
    uint32_t result;
    uint32_t temp;
    q40_rtc_data_t now;

    /*
        bit31:25 Year origin from the 1980 (0..127, e.g. 37 for 2017)
        bit24:21 Month (1..12)
        bit20:16 Day of the month (1..31)
        bit15:11 Hour (0..23)
        bit10:5  Minute (0..59)
        bit4:0   Second / 2 (0..29, e.g. 25 for 50)
    */

    q40_rtc_read_clock(&now);

    // printf("raw rtc: y=%02x m=%02x d=%02x h=%02x m=%02x s=%02x\n",
    //         now.year, now.month, now.day, now.hour, now.minute, now.second);
    temp = decode_bcd_byte(now.year);
    if(temp <= 80)
        temp += 20;
    result  = temp                                << 25;
    result |= decode_bcd_byte(now.month   & 0x1F) << 21;
    result |= decode_bcd_byte(now.day     & 0x3F) << 16;
    result |= decode_bcd_byte(now.hour    & 0x3F) << 11;  
    result |= decode_bcd_byte(now.minute  & 0x7F) <<  5;
    result |= decode_bcd_byte(now.second  & 0x7F) >>  1;

    return result;
}

void* ff_memalloc (UINT msize)
{
    return malloc(msize);
}

void ff_memfree (void* mblock)
{
    free(mblock);
}
