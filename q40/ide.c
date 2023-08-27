/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <ide.h>
#include <q40/ide.h>
#include <q40/isa.h>
#include <q40/hw.h>

// configurable options
#define MAX_IDE_DISKS FF_VOLUMES
#define NUM_CONTROLLERS 1
static const uint16_t controller_base_io_addr[] = {0x1f0,};

// debugging options:
#undef ATA_DUMP_IDENTIFY_RESULT

static disk_controller_t disk_controller[NUM_CONTROLLERS];
static disk_t disk_table[MAX_IDE_DISKS];
static int disk_table_used = 0;
static bool disk_init_done = false;

static void disk_controller_reset(disk_controller_t *ctrl)
{
    printf("IDE reset controller at 0x%x:", ctrl->base_io);
    *ctrl->device_reg = 0xE0; /* select master */
    *ctrl->ctl_reg    = 0x06; /* assert reset, no interrupts */
    delay_ms(100);
    *ctrl->ctl_reg    = 0x02; /* release reset, no interrupts */
    delay_ms(50);
    printf(" done\n");
}

static bool disk_wait(disk_controller_t *ctrl, uint8_t bits)
{
    uint8_t status;
    timer_t timeout;

    /* read alt status once to ensure we meet timing for reading status */
    status = *ctrl->altstatus_reg;

    timeout = set_timer_sec(3); 

    do{
        status = *ctrl->status_reg;

        if((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR | bits)) == bits)
            return true;

        if(((status & (IDE_STATUS_BUSY | IDE_STATUS_ERROR)) == IDE_STATUS_ERROR) ||
            (status == 0x00) || (status == 0xFF)){ /* error */
            return false;
        }
    }while(!timer_expired(timeout));

    printf("IDE timeout, status=%x\n", status);
    return false;
}

static void disk_data_read_sector_data(disk_controller_t *ctrl, void *ptr)
{
    uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *(buffer++) = __builtin_bswap16(*ctrl->data_reg);
    }
}

static void disk_data_write_sector_data(disk_controller_t *ctrl, const void *ptr)
{
    const uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *ctrl->data_reg = __builtin_bswap16(*(buffer++));
    }
}

static bool disk_data_readwrite(int disknr, void *buff, uint32_t sector, int sector_count, bool is_write)
{
    disk_t *disk;
    disk_controller_t *ctrl;
    int nsect;

    if(disknr < 0 || disknr >= disk_table_used){
        printf("bad disk %d\n", disknr);
        return false;
    }

    disk = &disk_table[disknr];
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
        if(!disk_wait(ctrl, IDE_STATUS_READY))
            return false;

        /* send command */
        *ctrl->command_reg = is_write ? IDE_CMD_WRITE_SECTOR : IDE_CMD_READ_SECTOR;

        /* transfer sectors */
        while(nsect > 0){
            /* unclear if we need to wait for DRQ on each sector? play it safe */
            if(!disk_wait(ctrl, IDE_STATUS_DATAREQUEST))
                return false;
            if(is_write)
                disk_data_write_sector_data(ctrl, buff);
            else
                disk_data_read_sector_data(ctrl, buff);
            buff += 512;
            nsect--;
        }

        if(is_write) /* wait for write operations to complete */
            if(!disk_wait(ctrl, IDE_STATUS_READY))
                return false;
    }

    return true;
}

static void disk_data_read_name(const uint8_t *id, char *buffer, int offset, int len)
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

static void disk_init_disk(disk_controller_t *ctrl, int disk)
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
    if(!disk_wait(ctrl, IDE_STATUS_READY)){
        printf("no disk found.\n");
        return;
    }

    /* send identify command */
    *ctrl->command_reg = IDE_CMD_IDENTIFY;

    if(!disk_wait(ctrl, IDE_STATUS_DATAREQUEST)){
        printf("disk not responding.\n");
	return;
    }

    disk_data_read_sector_data(ctrl, buffer);

    /* confirm disk has LBA support */
    if(!(buffer[99] & 0x02)) {
        printf("LBA not supported.\n");
        return;
    }

    /* read out the disk's sector count, name etc */
    sectors = le32_to_cpu(*((uint32_t*)&buffer[ATA_ID_LBA_CAPACITY]));
    disk_data_read_name(buffer, prod,   ATA_ID_PROD,   ATA_ID_PROD_LEN);

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

    if(disk_table_used >= MAX_IDE_DISKS){
        printf("Max disks reached\n");
    }else{
        char path[4];
        disk_table[disk_table_used].ctrl = ctrl;
        disk_table[disk_table_used].disk = disk;
        disk_table[disk_table_used].sectors = sectors;
        disk_table[disk_table_used].fat_fs_status = STA_NOINIT;

        /* prepare FatFs to talk to the volume */
        path[0] = '0' + disk_table_used;
        path[1] = ':';
        path[2] = 0;
        f_mount(&disk_table[disk_table_used].fat_fs_workarea, path, 0); /* lazy mount */

        disk_table_used++;
    }

    return;
}

static void disk_controller_init(disk_controller_t *ctrl, uint16_t base_io)
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

    disk_controller_reset(ctrl);
    for(int disk=0; disk<2; disk++){
        disk_init_disk(ctrl, disk);
    }
}

int disk_get_count(void)
{
    return disk_table_used;
}

disk_t *disk_get_info(int nr)
{
    if(nr < 0 || nr >= disk_get_count())
        return NULL;
    return &disk_table[nr];
}

bool disk_data_read(int disknr, void *buff, uint32_t sector, int sector_count)
{
    return disk_data_readwrite(disknr, buff, sector, sector_count, false);
}

bool disk_data_write(int disknr, const void *buff, uint32_t sector, int sector_count)
{
    return disk_data_readwrite(disknr, (void*)buff, sector, sector_count, true);
}

void disk_init(void)
{
    /* one per customer */
    if(disk_init_done){
        printf("disk_init: already done?\n");
        return;
    }
    disk_init_done = true;

    /* initialise controllers */
    for(int i=0; i<NUM_CONTROLLERS; i++){
        disk_controller_init(&disk_controller[i], controller_base_io_addr[i]);
    }
}
