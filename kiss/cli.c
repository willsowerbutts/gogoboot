#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <fatfs/ff.h>
#include <tinyalloc.h>
#include <cpu.h>
#include <cli.h>

const cmd_entry_t target_cmd_table[] = {
  /* name          min     max  function      help */
    //{"softrom",     0,      1,  &do_softrom,  "load and boot a Q40 ROM image" },
    //{"hardrom",     0,      0,  &do_hardrom,  "reboot into Q40 hardware ROM" },
    {0, 0, 0, 0, 0 } /* terminator */
};
