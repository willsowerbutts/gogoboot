/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <q40types.h>
#include "ff.h"
#include "diskio.h"
#include "q40ide.h"
#include "q40isa.h"
#include "q40hw.h"

// debugging options:
#undef ATA_DUMP_IDENTIFY_RESULT

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
    ide_controller_t *ctrl;
    int disk;               /* 0 = master, 1 = slave */
    uint32_t sectors;       /* 32 bits limits us to 2TB */
    DSTATUS fat_fs_status;
    FATFS fat_fs_workarea;
    /* name and model? */
    /* partition table? */
} ide_disk_t;

#define NUM_CONTROLLERS 1
static const uint16_t controller_base_io_addr[] = {0x1f0,};

static ide_controller_t ide_controller[NUM_CONTROLLERS];

#define MAX_IDE_DISKS FF_VOLUMES
static ide_disk_t ide_disk[MAX_IDE_DISKS];
int ide_disk_used = 0;
static bool q40_ide_init_done = false;

static void q40_ide_controller_reset(ide_controller_t *ctrl)
{
    printf("IDE reset controller at 0x%x:", ctrl->base_io);
    *ctrl->device_reg = 0xE0; /* select master */
    *ctrl->ctl_reg    = 0x06; /* assert reset, no interrupts */
    q40_delay(400000);
    *ctrl->ctl_reg    = 0x02; /* release reset, no interrupts */
    printf(" done\n");
}

static bool q40_ide_wait(ide_controller_t *ctrl, uint8_t bits)
{
    uint8_t status;
    int countdown = 4000000;

    /* read alt status once to ensure we meet timing for reading status */
    status = *ctrl->altstatus_reg;

    do{
        status = *ctrl->status_reg;

        if((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR | bits)) == bits)
            return true;

        if(((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR)) == IDE_STATUS_ERROR) ||
            (status == 0x00) || (status == 0xFF)){ /* error */
            return false;
        }
    }while(countdown--);

    printf("IDE timeout, status=%x\n", status);
    return false;
}

static void q40_ide_read_sector_data(ide_controller_t *ctrl, void *ptr)
{
    uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *(buffer++) = __builtin_bswap16(*ctrl->data_reg);
    }
}

static void q40_ide_write_sector_data(ide_controller_t *ctrl, const void *ptr)
{
    const uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *ctrl->data_reg = __builtin_bswap16(*(buffer++));
    }
}

int q40_ide_get_disk_count(void)
{
    return ide_disk_used;
}

static bool q40_ide_readwrite(int disknr, void *buff, uint32_t sector, int sector_count, bool is_write)
{
    ide_disk_t *disk;
    ide_controller_t *ctrl;
    int nsect;

    if(disknr < 0 || disknr >= ide_disk_used){
        printf("bad disk %d\n", disknr);
        return false;
    }

    disk = &ide_disk[disknr];
    ctrl = disk->ctrl;

    while(sector_count > 0){
        /* select device, program LBA */
        *ctrl->device_reg = (uint8_t)(((sector >> 24) & 0x0F) | (disk->disk == 0 ? 0xE0 : 0xF0));
        *ctrl->lbah_reg   = (uint8_t)( (sector >> 16) & 0xFF);
        *ctrl->lbam_reg   = (uint8_t)( (sector >>  8) & 0xFF);
        *ctrl->lbal_reg   = (uint8_t)( (sector      ) & 0xFF);

        if(sector_count >= 256)
            nsect = 256;
        else
            nsect = sector_count;

        /* setup for next loop */
        sector_count -= nsect;
        sector += nsect;

        /* program sector count */
        *ctrl->nsect_reg  = nsect == 256 ? 0 : nsect;

        /* wait for device to be ready */
        if(!q40_ide_wait(ctrl, IDE_STATUS_READY))
            return false;

        /* send command */
        *ctrl->command_reg = is_write ? IDE_CMD_WRITE_SECTOR : IDE_CMD_READ_SECTOR;

        /* read result */
        while(nsect > 0){
            /* unclear if we need to wait for DRQ on each sector? play it safe */
            if(!q40_ide_wait(ctrl, IDE_STATUS_DATAREQUEST))
                return false;
            if(is_write)
                q40_ide_write_sector_data(ctrl, buff);
            else
                q40_ide_read_sector_data(ctrl, buff);
            buff += 512;
            nsect--;
        }
    }

    return true;
}

bool q40_ide_read(int disknr, void *buff, uint32_t sector, int sector_count)
{
    return q40_ide_readwrite(disknr, buff, sector, sector_count, false);
}

bool q40_ide_write(int disknr, const void *buff, uint32_t sector, int sector_count)
{
    return q40_ide_readwrite(disknr, (void*)buff, sector, sector_count, true);
}

static void q40_ide_read_name(const uint8_t *id, char *buffer, int offset, int len)
{
    int rem;
    char *s;

    /* this reads the ASCII string out of the "identify" page pointed to by 'id'
     * 'buffer' must be at least 'len'+1 bytes */
    s = buffer;
    for(rem=len; rem>0; rem-=2){
        *(s++) = id[offset+1];
        *(s++) = id[offset];
        offset+=2;
    }
    *(s++) = 0;

    // remove trailing spaces -- like rtrim()
    s = buffer + strlen(buffer);
    while(s > buffer && s[-1] == ' ')
        s--;
    *s = 0;
}

static void q40_ide_disk_init(ide_controller_t *ctrl, int disk)
{
    uint8_t sel, buffer[512];
    char prod[1+ATA_ID_PROD_LEN];
    uint32_t sectors;

    printf("IDE probe 0x%x disk %d: ", ctrl->base_io, disk);

    switch(disk){
        case 0: sel = 0xE0; break;
        case 1: sel = 0xF0; break;
        default: 
            printf("bad disk %d?\n", disk);
            return;
    }

    *ctrl->device_reg = sel; /* select master/slave */

    /* wait for drive to be ready */
    if(!q40_ide_wait(ctrl, IDE_STATUS_READY)){
        printf("no disk found.\n");
        return;
    }

    /* send identify command */
    *ctrl->command_reg = IDE_CMD_IDENTIFY;

    if(!q40_ide_wait(ctrl, IDE_STATUS_DATAREQUEST)){
        printf("disk not responding.\n");
	return;
    }

    q40_ide_read_sector_data(ctrl, buffer);

    /* confirm disk has LBA support */
    if(!(buffer[99] & 0x02)) {
        printf("LBA not supported.\n");
        return;
    }

    /* read out the disk's sector count, name etc */
    sectors = le32_to_cpu(*((uint32_t*)&buffer[ATA_ID_LBA_CAPACITY]));
    q40_ide_read_name(buffer, prod,   ATA_ID_PROD,   ATA_ID_PROD_LEN);

    printf("%s (%lu sectors, %lu MB)\n", prod, sectors, sectors>>11);

#ifdef ATA_DUMP_IDENTIFY_RESULT
    for(int i=0; i<512; i+=16){
        for(int j=0; j<16; j++)
            printf("%02x ", buffer[i+j]);
        printf("    ");
        for(int j=0; j<16; j++)
            putch((buffer[i+j] >= 0x20 && buffer[i+j] < 0x7f) ? buffer[i+j] : '.');
        putch('\n');
    }
#endif

    if(ide_disk_used >= MAX_IDE_DISKS){
        printf("Max disks reached\n");
    }else{
        char path[4];
        ide_disk[ide_disk_used].ctrl = ctrl;
        ide_disk[ide_disk_used].disk = disk;
        ide_disk[ide_disk_used].sectors = sectors;
        ide_disk[ide_disk_used].fat_fs_status = STA_NOINIT;

        /* prepare FatFs to talk to the volume */
        path[0] = '0' + ide_disk_used;
        path[1] = ':';
        path[2] = 0;
        f_mount(&ide_disk[ide_disk_used].fat_fs_workarea, path, 0); /* lazy mount */

        ide_disk_used++;
    }

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
    /* one per customer */
    if(q40_ide_init_done){
        printf("q40_ide_init: already done?\n");
        return;
    }
    q40_ide_init_done = true;

    /* initialise controllers */
    for(int i=0; i<NUM_CONTROLLERS; i++){
        q40_ide_controller_init(&ide_controller[i], controller_base_io_addr[i]);
    }
}

/* glue for FatFs library */

DSTATUS disk_status(BYTE pdrv)
{
    if(pdrv >= ide_disk_used)
        return STA_NOINIT;
    return ide_disk[pdrv].fat_fs_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if(pdrv >= ide_disk_used)
        return STA_NOINIT;
    // assume success
    ide_disk[pdrv].fat_fs_status = 0;
    return ide_disk[pdrv].fat_fs_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if(pdrv >= ide_disk_used)
        return RES_PARERR;
    if(ide_disk[pdrv].fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;
    if(q40_ide_read(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if(pdrv >= ide_disk_used)
        return RES_PARERR;
    if(ide_disk[pdrv].fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;
    if(ide_disk[pdrv].fat_fs_status & STA_PROTECT)
        return RES_WRPRT;
    if(q40_ide_write(pdrv, buff, sector, count))
        return RES_OK;
    else
        return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if(pdrv >= ide_disk_used)
        return RES_PARERR;
    if(ide_disk[pdrv].fat_fs_status & (STA_NOINIT | STA_NODISK))
        return RES_NOTRDY;
    switch(cmd){
        case CTRL_SYNC:
        case CTRL_TRIM:
            return RES_OK;
        case GET_SECTOR_SIZE:
            *((long*)buff) = 512;
            return RES_OK;
        case GET_SECTOR_COUNT:
            *((long*)buff) = ide_disk[pdrv].sectors;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
