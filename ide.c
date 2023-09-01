/* 2023-08-20 William R Sowerbutts */

#include <stdlib.h>
#include <timers.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <ide.h>

#define MAX_IDE_DISKS FF_VOLUMES

// debugging option:
#undef ATA_DUMP_IDENTIFY_RESULT

static disk_t **disk_table = 0;
static int disk_table_size = 0;

static bool ide_wait_status(disk_controller_t *ctrl, uint8_t bits)
{
    uint8_t status;
    timer_t timeout;

    /* read alt status once to ensure we meet timing for reading status */
    status = ide_get_register(ctrl, ATA_REG_ALTSTATUS);

    timeout = set_timer_sec(3); 

    do{
        status = ide_get_register(ctrl, ATA_REG_STATUS);

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

static bool disk_data_readwrite(int disknr, void *buff, uint32_t sector, int sector_count, bool is_write)
{
    disk_t *disk;
    disk_controller_t *ctrl;
    int nsect;

    if(disknr < 0 || disknr >= disk_table_size){
        printf("bad disk %d\n", disknr);
        return false;
    }

    disk = disk_table[disknr];
    ctrl = disk->ctrl;

    //printf("disk %d op=%s sector=%ld count=%d sectors\n",
    //        disknr, is_write?"write":"read", sector, sector_count);

    while(sector_count > 0){
        /* select device, program LBA */
        ide_set_register(ctrl, ATA_REG_DEVICE, (((sector >> 24) & 0x0F) | (disk->disk == 0 ? 0xE0 : 0xF0)));
        ide_set_register(ctrl, ATA_REG_LBAH,   ( (sector >> 16) & 0xFF));
        ide_set_register(ctrl, ATA_REG_LBAM,   ( (sector >>  8) & 0xFF));
        ide_set_register(ctrl, ATA_REG_LBAL,   ( (sector      ) & 0xFF));

        if(sector_count >= 256)
            nsect = 256;
        else
            nsect = sector_count;

        /* setup for next loop */
        sector_count -= nsect;
        sector += nsect;

        /* program sector count */
        ide_set_register(ctrl, ATA_REG_NSECT, nsect == 256 ? 0 : nsect);

        /* wait for device to be ready */
        if(!ide_wait_status(ctrl, IDE_STATUS_READY))
            return false;

        /* send command */
        ide_set_register(ctrl, ATA_REG_CMD, is_write ? IDE_CMD_WRITE_SECTOR : IDE_CMD_READ_SECTOR);

        /* read result */
        while(nsect > 0){
            /* unclear if we need to wait for DRQ on each sector? play it safe */
            if(!ide_wait_status(ctrl, IDE_STATUS_DATAREQUEST))
                return false;
            if(is_write)
                ide_transfer_sector_write(ctrl, buff);
            else
                ide_transfer_sector_read(ctrl, buff);
            buff += 512;
            nsect--;
        }

        if(is_write) /* wait for write operations to complete */
            if(!ide_wait_status(ctrl, IDE_STATUS_READY))
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

static void disk_init_disk(disk_controller_t *ctrl, int drivenr)
{
    uint8_t sel, buffer[512];
    char prod[1+ATA_ID_PROD_LEN];
    uint32_t sectors;

    printf("  Probe disk %d: ", drivenr);

    switch(drivenr){
        case 0: sel = 0xE0; break;
        case 1: sel = 0xF0; break;
        default: 
            printf("bad disk %d?\n", drivenr);
            return;
    }

    ide_set_register(ctrl, ATA_REG_DEVICE, sel); /* select master/slave */

    /* wait for drive to be ready */
    if(!ide_wait_status(ctrl, IDE_STATUS_READY)){
        printf("no disk found.\n");
        return;
    }

    /* send identify command */
    ide_set_register(ctrl, ATA_REG_CMD, IDE_CMD_IDENTIFY);

    if(!ide_wait_status(ctrl, IDE_STATUS_DATAREQUEST)){
        printf("disk not responding.\n");
	return;
    }

    ide_transfer_sector_read(ctrl, buffer);

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

    if(disk_table_size >= MAX_IDE_DISKS){
        printf("Max disks reached\n");
    }else{
        char path[4];
        disk_t *disk;

        disk = malloc(sizeof(disk_t));
        disk_table = realloc(disk_table, sizeof(disk_t*) * (disk_table_size + 1));
        disk_table[disk_table_size] = disk;

        disk->ctrl = ctrl;
        disk->disk = drivenr;
        disk->sectors = sectors;
        disk->fat_fs_status = STA_NOINIT;

        /* prepare FatFs to talk to the volume */
        path[0] = '0' + disk_table_size;
        path[1] = ':';
        path[2] = 0;

        f_mount(&disk->fat_fs_workarea, path, 0); /* lazy mount */

        disk_table_size++;
    }

    return;
}

void disk_controller_startup(disk_controller_t *ctrl)
{
    /* reset attached devices */
    ide_set_register(ctrl, ATA_REG_DEVICE, 0xE0);   /* select master */
    ide_set_register(ctrl, ATA_REG_ALTSTATUS, 0x06); /* assert reset, no interrupts */
    delay_ms(50);
    ide_set_register(ctrl, ATA_REG_ALTSTATUS, 0x02); /* release reset, no interrupts */
    delay_ms(200);

    for(int disk=0; disk<2; disk++){
        disk_init_disk(ctrl, disk);
    }
}

int disk_get_count(void)
{
    return disk_table_size;
}

disk_t *disk_get_info(int nr)
{
    if(nr < 0 || nr >= disk_get_count())
        return NULL;
    return disk_table[nr];
}

bool disk_data_read(int disknr, void *buff, uint32_t sector, int sector_count)
{
    return disk_data_readwrite(disknr, buff, sector, sector_count, false);
}

bool disk_data_write(int disknr, const void *buff, uint32_t sector, int sector_count)
{
    return disk_data_readwrite(disknr, (void*)buff, sector, sector_count, true);
}
