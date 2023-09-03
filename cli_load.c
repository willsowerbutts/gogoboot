/* Copyright (C) 2015-2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <cli.h>
#include <net.h>
#include <fatfs/ff.h>

void do_execute(char *argv[], int argc)
{
    uint32_t address;
    address = strtoul(argv[0], NULL, 0);
    execute((void*)address);
}

void do_load(char *argv[], int argc)
{
    FIL fd;
    FRESULT fr;
    uint32_t address, fsize, offset=0, msize=0;

    /* arg 1 - filename */
    fr = f_open(&fd, argv[0], FA_READ);
    if(fr != FR_OK){
        printf("load: failed to open \"%s\": %s (aborted)\n", argv[0], f_errmsg(fr));
        return;
    }

    /* arg 2 - load address */
    address = strtoul(argv[1], NULL, 0);

    msize = fsize = f_size(&fd);

    /* arg 3 - file offset */
    if(argc >= 3){
        offset = strtoul(argv[2], NULL, 0);
    }

    /* arg 4 - load length */
    if(argc >= 4){
        msize = strtoul(argv[3], NULL, 0);
    }

    if(offset > fsize){
        printf("load: offset 0x%lx exceeds file size 0x%lx (aborted)\n", offset, fsize);
    }else{
        fsize -= offset;
        if(fsize > msize)
            fsize = msize;
        load_data(&fd, address, offset, fsize, msize);
    }

    f_close(&fd);
}
