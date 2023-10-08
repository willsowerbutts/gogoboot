#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <q40/hw.h>
#include <fatfs/ff.h>
#include <cpu.h>
#include <cli.h>

static void do_loadimage(char *argv[], int argc)
{
    FRESULT fr;
    FIL fd;

    fr = f_open(&fd, argv[0], FA_READ);
    if(fr == FR_OK){
        f_read(&fd, (char*)VIDEO_RAM_BASE, (1024*1024), NULL);
        f_close(&fd);
    }
    cpu_cache_flush(); /* flush data to video memory */
}

static void do_hardrom(char *argv[], int argc)
{
    printf("hardrom: rebooting ...\n\n\n");
    q40_boot_softrom(0);
}

static void do_softrom(char *argv[], int argc)
{
    FRESULT fr;
    UINT loaded;
    FIL fd;
    char *romimage = malloc(Q40_ROMSIZE);

    if(argc == 0){
        /* load from UART */
        uint32_t count;
        printf("softrom: loading from UART ...\n");
        uart_read_string(&count, sizeof(count));
        if(count > Q40_ROMSIZE){
            printf("softrom: too big (%ld)!\n", count);
            free(romimage);
            return;
        }
        uart_read_string(romimage, count);
        loaded = count;
    }else{
        /* load from file */
        fr = f_open(&fd, argv[0], FA_READ);
        if(fr != FR_OK){
            free(romimage);
            printf("softrom: failed to read \"%s\": ", argv[0]);
            f_perror(fr);
            return;
        }
        f_read(&fd, romimage, Q40_ROMSIZE, &loaded);
        f_close(&fd);
        printf("softrom: loaded %d bytes from \"%s\"\n", loaded, argv[0]);
    }

    // pad with 0xFF
    memset(romimage+loaded, 0xFF, Q40_ROMSIZE-loaded);

    // check if what we've been given is different to what is already loaded
    if(memcmp(romimage, rom_pointer, Q40_ROMSIZE)){
        printf("softrom: rebooting ...\n\n\n");

        // jump to assembler magic
        q40_boot_softrom(romimage);
    }else{
        printf("softrom: matches running ROM\n");
    } 

    free(romimage);
}

const cmd_entry_t target_cmd_table[] = {
  /* name          min     max  function      help */
    {"softrom",     0,      1,  &do_softrom,  "load and boot a Q40 ROM image" },
    {"hardrom",     0,      0,  &do_hardrom,  "reboot into Q40 hardware ROM" },
    {"loadimage",   1,      1,  &do_loadimage,"load image into Q40 graphics memory" },
    {0, 0, 0, 0, 0 } /* terminator */
};
