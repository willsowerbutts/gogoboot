/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <q40types.h>
#include "ff.h"
#include "diskio.h"
#include "q40ide.h"
#include "q40isa.h"
#include "q40hw.h"

typedef struct {
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

typedef struct {
    bool present;
    ide_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sector_count;   /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
    /* name and model? */
    /* partition table? */
} ide_disk_t;

#define NUM_CONTROLLERS 1
static const uint16_t controller_base_io_addr[] = {0x1f0,};

static ide_controller_t ide_controller[NUM_CONTROLLERS];

static ide_disk_t disk_info[FF_VOLUMES];
static int q40_ide_init_done = 0;

static void q40_ide_controller_reset(ide_controller_t *ctrl)
{
    printf("ide reset 0x%x:", ctrl->base_io);
    *ctrl->device_reg = 0xE0; /* select master */
    *ctrl->ctl_reg    = 0x06; /* assert reset, no interrupts */
    q40_delay(1000000);
    *ctrl->ctl_reg    = 0x02; /* release reset, no interrupts */
    printf(" done\n");
}

static bool q40_ide_wait(ide_controller_t *ctrl, uint8_t bits)
{
    uint8_t status;
    int countdown = 4000000;
    int ps = -1;

    do{
        status = *ctrl->status_reg;

        if(status != ps){
            ps = status;
            printf("[%02x]", status);
        }

        if((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR | bits)) == bits)
            return true;

        if(((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR)) == IDE_STATUS_ERROR)){ /* error */
            printf("ide error, status=%x\n", status);
            return false;
        }
    }while(countdown--);

    printf("ide timeout, status=%x\n", status);
    return false;
}

static void q40_ide_read_sector(ide_controller_t *ctrl, void *ptr)
{
    uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *(buffer++) = __builtin_bswap16(*ctrl->data_reg);
    }
}

static void q40_ide_disk_init(ide_controller_t *ctrl, int disk)
{
    uint8_t sel, buffer[512];
    uint32_t sectors;

    switch(disk){
        case 0: sel = 0xE0; break;
        case 1: sel = 0xF0; break;
        default: 
            printf("bad disk %d?\n", disk);
            return;
    }

    *ctrl->device_reg = sel; /* select master/slave */

    /* confirm disk has LBA support */
    if(!q40_ide_wait(ctrl, IDE_STATUS_READY))
        return;

    /* send identify command */
    *ctrl->command_reg = IDE_CMD_IDENTIFY;

    if(!q40_ide_wait(ctrl, IDE_STATUS_DATAREQUEST))
	return;

    q40_ide_read_sector(ctrl, buffer);

    putch('\n');

    for(int i=0; i<512; i+=16){
        for(int j=0; j<16; j++)
            printf("%02x ", buffer[i+j]);
        printf("    ");
        for(int j=0; j<16; j++)
            putch((buffer[i+j] >= 0x20 && buffer[i+j] < 0x7f) ? buffer[i+j] : '.');
        putch('\n');
    }

    if(!(buffer[99] & 0x02)) {
        printf("LBA unsupported.\n");
        return;
    }

    /* read out the disk's sector count */
    sectors = le32_to_cpu(*((uint32_t*)&buffer[120]));

    printf("%lu sectors (%lu MB)\n", sectors, sectors>>11);

    /* scan partitions */
    //blkdev_scan(blk, SWAPSCAN);

    return;
}

static void q40_ide_controller_init(ide_controller_t *ctrl, uint16_t base_io)
{
    /* set up controller register pointers */
    ctrl->base_io = base_io;

    ctrl->ctl_reg     = ISA_XLATE_ADDR_BYTE(base_io + 0x206);
    ctrl->altstatus_reg = ctrl->ctl_reg;

    ctrl->error_reg   = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_ERR);
    ctrl->feature_reg = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_FEATURE);

    ctrl->nsect_reg   = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_NSECT);
    ctrl->lbal_reg    = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_LBAL);
    ctrl->lbam_reg    = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_LBAM);
    ctrl->lbah_reg    = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_LBAH);

    ctrl->device_reg  = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_DEVICE); // ATA_REG_DEVSEL, IDE_REG_DEVHEAD

    ctrl->status_reg  = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_STATUS);
    ctrl->command_reg = ISA_XLATE_ADDR_BYTE(base_io + ATA_REG_CMD);

    ctrl->data_reg    = ISA_XLATE_ADDR_WORD(base_io + ATA_REG_DATA);

    q40_ide_controller_reset(ctrl);
    for(int disk=0; disk<2; disk++){
        q40_ide_disk_init(ctrl, disk);
    }

}

void q40_ide_init(void)
{
    char path[4];

    printf("q40_ide_init() done=%x\n", q40_ide_init_done);

#if 0
    /* one per customer */
    if(q40_ide_init_done)
        return;
    q40_ide_init_done = 1;
#endif

    printf("q40_ide_init()\n");

    /* reset status */
    for(int i=0; i<FF_VOLUMES; i++){
        disk_info[i].fat_fs_status = STA_NOINIT;
        disk_info[i].ctrl = NULL;
        disk_info[i].disk = -1;
        disk_info[i].sector_count = 0;
        disk_info[i].present = false;
    }

    /* initialise controllers */
    for(int i=0; i<NUM_CONTROLLERS; i++){
        q40_ide_controller_init(&ide_controller[i], controller_base_io_addr[i]);
    }

    /* prepare FatFs to talk to the volumes */
    for(int i=0; i<FF_VOLUMES; i++){
        path[0] = '0' + i;
        path[1] = ':';
        path[2] = 0;
        f_mount(&disk_info[i].fat_fs_workarea, path, 0); /* lazy mount */
    }
}

DSTATUS disk_status (BYTE pdrv)
{
    if(pdrv >= FF_VOLUMES)
        return STA_NOINIT;
    else
        return disk_info[pdrv].fat_fs_status;
}

DSTATUS disk_initialize (BYTE pdrv)
{
    return STA_NOINIT;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    q40_ide_read_sector(&ide_controller[0], buff);
    return RES_PARERR;
}

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    return RES_PARERR;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
    return RES_PARERR;
}
