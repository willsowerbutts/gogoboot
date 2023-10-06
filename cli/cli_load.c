/* Copyright (C) 2015-2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <cli.h>
#include <net.h>
#include <fatfs/ff.h>

void do_execute(char *argv[], int argc)
{
    uint32_t address;
    address = parse_uint32(argv[0], NULL);
    execute((void*)address, argc-1, argv+1);
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
    address = parse_uint32(argv[1], NULL);

    msize = fsize = f_size(&fd);

    /* arg 3 - file offset */
    if(argc >= 3){
        offset = parse_uint32(argv[2], NULL);
    }

    /* arg 4 - load length */
    if(argc >= 4){
        msize = parse_uint32(argv[3], NULL);
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

void do_save(char *argv[], int argc)
{
    FIL fd;
    FRESULT fr;
    uint32_t address, msize;

    /* arg 1 - filename */
    fr = f_open(&fd, argv[0], FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK){
        printf("save: failed to open \"%s\": %s (aborted)\n", argv[0], f_errmsg(fr));
        return;
    }

    /* arg 2 - load address */
    address = parse_uint32(argv[1], NULL);

    /* arg 3 - save length */
    msize = parse_uint32(argv[2], NULL);

    printf("save: writing 0x%lx bytes from 0x%lx to \"%s\"\n", msize, address, argv[0]);

    fr = f_write(&fd, (void*)address, msize, NULL);
    if(fr != FR_OK){
        printf("save: failed to write to \"%s\": %s\n", argv[0], f_errmsg(fr));
    }

    f_close(&fd);
}
