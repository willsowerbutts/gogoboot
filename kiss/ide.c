/* 2023-07-25 William R Sowerbutts
 * Includes some code from my "devide" FUZIX IDE driver.  */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <q40/isa.h>
#include <q40/hw.h>

int gogoboot_disk_get_disk_count(void)
{
    return 0;
}

bool gogoboot_disk_read(int disknr, void *buff, uint32_t sector, int sector_count)
{
    return false;
}

bool gogoboot_disk_write(int disknr, const void *buff, uint32_t sector, int sector_count)
{
    return false;
}

void gogoboot_disk_init(void)
{
}

disk_t *gogoboot_get_disk_info(int nr)
{
    return NULL;
}
