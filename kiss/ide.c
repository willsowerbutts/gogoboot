/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <timers.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <kiss/ide.h>
#include <kiss/ecb.h>

// configurable options
#define MAX_IDE_DISKS FF_VOLUMES
#define NUM_CONTROLLERS 2
static const uint16_t controller_base_io_addr[] = { KISS68030_MFPIC_8255,        /* primary MF/PIC */
                                                    KISS68030_MFPIC_8255+0x10 }; /* secondary MF/PIC */

// debugging options:
#undef ATA_DUMP_IDENTIFY_RESULT

static disk_controller_t disk_controller[NUM_CONTROLLERS];
static disk_t disk_table[MAX_IDE_DISKS];
static int disk_table_used = 0;
static bool disk_init_done = false;

static void ide_set_data_direction(disk_controller_t *ctrl, bool read_mode)
{
    if(ctrl->read_mode != read_mode){
        *ctrl->control = read_mode ? PPIDE_PPI_BUS_READ : PPIDE_PPI_BUS_WRITE;
        ctrl->read_mode = read_mode;
    }
}

static uint8_t ide_get_register(disk_controller_t *ctrl, uint8_t reg)
{
    uint8_t val;
    ide_set_data_direction(ctrl, true);
    *ctrl->select = reg;
    *ctrl->control = 1 | (PPIDE_RD_BIT << 1);
    val = *ctrl->lsb;
    *ctrl->control = 0 | (PPIDE_RD_BIT << 1);
    return val;
}

static void ide_set_register(disk_controller_t *ctrl, uint8_t reg, uint8_t val)
{
    ide_set_data_direction(ctrl, false);
    *ctrl->select = reg;
    *ctrl->lsb = val;
    *ctrl->control = 1 | (PPIDE_WR_BIT << 1);
    *ctrl->control = 0 | (PPIDE_WR_BIT << 1);
}

static void disk_controller_reset(disk_controller_t *ctrl)
{
    printf("PPIDE reset controller at 0x%x:", ctrl->base_io);

    ide_set_register(ctrl, PPIDE_REG_DEVHEAD, 0xE0);   /* select master */
    ide_set_register(ctrl, PPIDE_REG_ALTSTATUS, 0x06); /* assert reset, no interrupts */
    delay_ms(100);
    ide_set_register(ctrl, PPIDE_REG_ALTSTATUS, 0x02); /* release reset, no interrupts */
    delay_ms(50);
    printf(" done\n");
}

static bool disk_wait(disk_controller_t *ctrl, uint8_t bits)
{
    uint8_t status;
    timer_t timeout;

    /* read alt status once to ensure we meet timing for reading status */
    status = ide_get_register(ctrl, PPIDE_REG_ALTSTATUS);

    timeout = set_timer_sec(3); 

    do{
        status = ide_get_register(ctrl, PPIDE_REG_STATUS);

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
    //printf("read sector\n");
    ide_set_data_direction(ctrl, true);
    *ctrl->select = PPIDE_REG_DATA;
    ide_sector_xfer_input(ptr, ctrl->lsb);
}

static void disk_data_write_sector_data(disk_controller_t *ctrl, const void *ptr)
{
    //printf("write sector\n");
    ide_set_data_direction(ctrl, false);
    *ctrl->select = PPIDE_REG_DATA;
    ide_sector_xfer_output(ptr, ctrl->lsb);
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

    //printf("disk %d op=%s sector=%ld count=%d sectors\n",
    //        disknr, is_write?"write":"read", sector, sector_count);

    while(sector_count > 0){
        /* select device, program LBA */
        ide_set_register(ctrl, PPIDE_REG_DEVHEAD, (((sector >> 24) & 0x0F) | (disk->disk == 0 ? 0xE0 : 0xF0)));
        ide_set_register(ctrl, PPIDE_REG_LBA_2,   ( (sector >> 16) & 0xFF));
        ide_set_register(ctrl, PPIDE_REG_LBA_1,   ( (sector >>  8) & 0xFF));
        ide_set_register(ctrl, PPIDE_REG_LBA_0,   ( (sector      ) & 0xFF));

        if(sector_count >= 256)
            nsect = 256;
        else
            nsect = sector_count;

        /* setup for next loop */
        sector_count -= nsect;
        sector += nsect;

        /* program sector count */
        ide_set_register(ctrl, PPIDE_REG_SEC_COUNT, nsect == 256 ? 0 : nsect);

        /* wait for device to be ready */
        if(!disk_wait(ctrl, IDE_STATUS_READY))
            return false;

        /* send command */
        ide_set_register(ctrl, PPIDE_REG_COMMAND, is_write ? IDE_CMD_WRITE_SECTOR : IDE_CMD_READ_SECTOR);

        /* read result */
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

    printf("PPIDE probe 0x%x disk %d: ", ctrl->base_io, disk);

    switch(disk){
        case 0: sel = 0xE0; break;
        case 1: sel = 0xF0; break;
        default: 
            printf("bad disk %d?\n", disk);
            return;
    }

    ide_set_register(ctrl, PPIDE_REG_DEVHEAD, sel); /* select master/slave */

    /* wait for drive to be ready */
    if(!disk_wait(ctrl, IDE_STATUS_READY)){
        printf("no disk found.\n");
        return;
    }

    /* send identify command */
    ide_set_register(ctrl, PPIDE_REG_COMMAND, IDE_CMD_IDENTIFY);

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
    ctrl->lsb     = &ECB_DEVICE_IO[base_io + PPIDE_LSB];
    ctrl->msb     = &ECB_DEVICE_IO[base_io + PPIDE_MSB];
    ctrl->select  = &ECB_DEVICE_IO[base_io + PPIDE_SIGNALS];
    ctrl->control = &ECB_DEVICE_IO[base_io + PPIDE_CONTROL];

    /* force a change in input mode to force configuration of the 8255 */
    ctrl->read_mode = false;
    ide_set_data_direction(ctrl, true);

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
