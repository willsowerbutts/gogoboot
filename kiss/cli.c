#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <fatfs/ff.h>
#include <kiss/ecb.h>
#include <kiss/hw.h>
#include <tinyalloc.h>
#include <cpu.h>
#include <cli.h>

extern const char text_start;

static void do_hardrom(char *argv[], int argc)
{
    printf("hardrom: rebooting ...\n\n\n");
    kiss_boot_softrom((void*)KISS68030_ROM_BASE);
}

static void do_softrom(char *argv[], int argc)
{
    FRESULT fr;
    char *romimage;
    UINT loaded;
    uint32_t count;
    FIL fd;

    if(argc == 0){
        /* load from UART */
        printf("softrom: loading from UART ...\n");
        uart_read_string(&count, sizeof(count));
        count--; // match bug in Q40 SOFTROM (yes even on KISS...): load one byte less than instructed
    }else{
        /* load from file */
        fr = f_open(&fd, argv[0], FA_READ);
        if(fr != FR_OK){
            printf("softrom: failed to read \"%s\": ", argv[0]);
            f_perror(fr);
            return;
        }
        count = f_size(&fd);
    }

    if(count > KISS68030_ROM_SIZE){
        printf("softrom: too big (%ld)!\n", count);
        if(argc != 0)
            f_close(&fd);
        return;
    }

    romimage = malloc(count);
    if(argc == 0){
        uart_read_string(romimage, count);
        loaded = count;
    }else{
        f_read(&fd, romimage, count, &loaded);
        f_close(&fd);
        printf("softrom: loaded %d bytes from \"%s\"\n", loaded, argv[0]);
    }

    // check if what we've been given is different to what is already loaded
    if(memcmp(romimage, &text_start, count)){
        printf("softrom: rebooting ...\n\n\n");

        // jump to assembler magic
        kiss_boot_softrom(romimage);
    }else{
        printf("softrom: matches running ROM\n");
    } 

    free(romimage);
}

const cmd_entry_t target_cmd_table[] = {
  /* name          min     max  function      help */
    {"softrom",     0,      1,  &do_softrom,  "load and boot a KISS ROM image" },
    {"hardrom",     0,      0,  &do_hardrom,  "reboot into KISS hardware ROM" },
    {0, 0, 0, 0, 0 } /* terminator */
};