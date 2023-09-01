/* 2023-08-20 William R Sowerbutts */

#include <stdlib.h>
#include <types.h>
#include <fatfs/ff.h>
#include <fatfs/diskio.h>
#include <disk.h>
#include <q40/hw.h>

/* glue for FatFs library */

DWORD get_fattime (void)
{
    return 0; /* fake it 'til we make it */
}
