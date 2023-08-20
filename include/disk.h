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

void gogoboot_disk_init(void);
disk_t *gogoboot_get_disk_info(int nr);
int gogoboot_disk_get_disk_count(void);
bool gogoboot_disk_read(int disk, void *buff, uint32_t sector, int sector_count);
bool gogoboot_disk_write(int disk, const void *buff, uint32_t sector, int sector_count);

/* IDE status register bits */
#define IDE_STATUS_BUSY         0x80
#define IDE_STATUS_READY        0x40
#define IDE_STATUS_DEVFAULT     0x20
#define IDE_STATUS_SEEKCOMPLETE 0x10 // not important
#define IDE_STATUS_DATAREQUEST  0x08
#define IDE_STATUS_CORRECTED    0x04 // not important
#define IDE_STATUS_INDEX        0x02 // not important
#define IDE_STATUS_ERROR        0x01

/* IDE command codes */
#define IDE_CMD_READ_SECTOR     0x20
#define IDE_CMD_WRITE_SECTOR    0x30
#define IDE_CMD_FLUSH_CACHE     0xE7
#define IDE_CMD_IDENTIFY        0xEC
#define IDE_CMD_SET_FEATURES    0xEF

/* excerpted from linux kernel include/linux/ata.h */
enum {  
        /* ATA command block registers */
        ATA_REG_DATA            = 0x00,
        ATA_REG_ERR             = 0x01,
        ATA_REG_NSECT           = 0x02,
        ATA_REG_LBAL            = 0x03,
        ATA_REG_LBAM            = 0x04,
        ATA_REG_LBAH            = 0x05,
        ATA_REG_DEVICE          = 0x06,
        ATA_REG_STATUS          = 0x07,

        ATA_REG_FEATURE         = ATA_REG_ERR, /* and their aliases */
        ATA_REG_CMD             = ATA_REG_STATUS,
        ATA_REG_BYTEL           = ATA_REG_LBAM,
        ATA_REG_BYTEH           = ATA_REG_LBAH,
        ATA_REG_DEVSEL          = ATA_REG_DEVICE,
        ATA_REG_IRQ             = ATA_REG_NSECT,

        /* identity page offsets and lengths (in bytes, not words) */
        ATA_ID_FW_REV       = 2*23,
        ATA_ID_FW_REV_LEN   = 8,
        ATA_ID_PROD         = 2*27,
        ATA_ID_PROD_LEN     = 40,
        ATA_ID_SERNO        = 2*10,
        ATA_ID_SERNO_LEN    = 20,
        ATA_ID_MAX_MULTSECT = 2*47,
        ATA_ID_MULTSECT     = 2*59,
        ATA_ID_LBA_CAPACITY = 2*60,
};

#endif
