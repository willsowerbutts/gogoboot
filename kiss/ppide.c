/* 2023-08-20 William R Sowerbutts */

#include <stdlib.h>
#include <timers.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <ide.h>
#include <kiss/ide.h>
#include <kiss/ecb.h>

#define NUM_CONTROLLERS 2
static disk_controller_t disk_controller[NUM_CONTROLLERS];
static const uint16_t controller_base_io_addr[] = { KISS68030_MFPIC_8255,        /* primary MF/PIC */
                                                    KISS68030_MFPIC_8255+0x10 }; /* secondary MF/PIC */

static void ide_set_data_direction(disk_controller_t *ctrl, bool read_mode)
{
    if(ctrl->read_mode != read_mode){
        *ctrl->control = read_mode ? PPIDE_PPI_BUS_READ : PPIDE_PPI_BUS_WRITE;
        ctrl->read_mode = read_mode;
    }
}

static void ide_select_register(disk_controller_t *ctrl, int reg)
{
    switch(reg){
        case ATA_REG_DATA:
            *ctrl->select = PPIDE_REG_DATA;
            break;
        case ATA_REG_ERR:
            *ctrl->select = PPIDE_REG_ERROR;
            break;
        case ATA_REG_NSECT:
            *ctrl->select = PPIDE_REG_SEC_COUNT;
            break;
        case ATA_REG_LBAL:
            *ctrl->select = PPIDE_REG_LBA_0;
            break;
        case ATA_REG_LBAM:
            *ctrl->select = PPIDE_REG_LBA_1;
            break;
        case ATA_REG_LBAH:
            *ctrl->select = PPIDE_REG_LBA_2;
            break;
        case ATA_REG_DEVICE:
            *ctrl->select = PPIDE_REG_DEVHEAD;
            break;
        case ATA_REG_STATUS:
            *ctrl->select = PPIDE_REG_STATUS;
            break;
        case ATA_REG_ALTSTATUS:
            *ctrl->select = PPIDE_REG_ALTSTATUS;
            break;
        default:
            printf("ide: bad reg %d?\n", reg);
            break;
    }
}

uint8_t ide_get_register(disk_controller_t *ctrl, int reg)
{
    uint8_t val;
    ide_set_data_direction(ctrl, true);
    ide_select_register(ctrl, reg);
    *ctrl->control = 1 | (PPIDE_RD_BIT << 1);
    val = *ctrl->lsb;
    *ctrl->control = 0 | (PPIDE_RD_BIT << 1);
    return val;
}

void ide_set_register(disk_controller_t *ctrl, int reg, uint8_t val)
{
    ide_set_data_direction(ctrl, false);
    ide_select_register(ctrl, reg);
    *ctrl->lsb = val;
    *ctrl->control = 1 | (PPIDE_WR_BIT << 1);
    *ctrl->control = 0 | (PPIDE_WR_BIT << 1);
}

void ide_transfer_sector_read(disk_controller_t *ctrl, void *ptr)
{
    ide_set_data_direction(ctrl, true);
    *ctrl->select = PPIDE_REG_DATA;
    ide_sector_xfer_input(ptr, ctrl->lsb);
}

void ide_transfer_sector_write(disk_controller_t *ctrl, const void *ptr)
{
    ide_set_data_direction(ctrl, false);
    *ctrl->select = PPIDE_REG_DATA;
    ide_sector_xfer_output(ptr, ctrl->lsb);
}

static void ide_controller_init(disk_controller_t *ctrl, uint16_t base_io)
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

    printf("PPIDE controller at 0x%x:\n", base_io);
    disk_controller_startup(ctrl);
}

void disk_init(void)
{
    /* initialise controllers */
    for(int i=0; i<NUM_CONTROLLERS; i++){
        ide_controller_init(&disk_controller[i], controller_base_io_addr[i]);
    }

}
