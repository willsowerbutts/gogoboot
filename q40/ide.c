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

#define NUM_CONTROLLERS 2
static disk_controller_t disk_controller[NUM_CONTROLLERS];
static const uint32_t controller_base_io_addr[] = { 0x1f0, 0x170 };

void ide_transfer_sector_read(disk_controller_t *ctrl, void *ptr)
{
    uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *(buffer++) = __builtin_bswap16(*ctrl->data_reg);
    }
}

void ide_transfer_sector_write(disk_controller_t *ctrl, const void *ptr)
{
    const uint16_t *buffer = ptr;

    for(int i=0; i<256; i++){
        *ctrl->data_reg = __builtin_bswap16(*(buffer++));
    }
}

uint8_t ide_get_register(disk_controller_t *ctrl, int reg)
{
    return isa_read_byte(ctrl->base_io + reg);
}

void ide_set_register(disk_controller_t *ctrl, int reg, uint8_t val)
{
    isa_write_byte(ctrl->base_io + reg, val);
}

static void ide_controller_init(disk_controller_t *ctrl, uint16_t base_io)
{
    /* set up controller register pointers */
    ctrl->base_io = base_io;
    ctrl->data_reg = ISA_XLATE_ADDR_WORD(base_io + ATA_REG_DATA);

    printf("IDE controller at 0x%x:\n", base_io);
    disk_controller_startup(ctrl);
}

void disk_init(void)
{
    /* initialise controllers */
    for(int i=0; i<NUM_CONTROLLERS; i++){
        ide_controller_init(&disk_controller[i], controller_base_io_addr[i]);
    }

}

