#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <fatfs/ff.h>
#include <tinyalloc.h>
#include <cpu.h>
#include <cli.h>

const cmd_entry_t target_cmd_table[] = {
  /* name          min     max  function      help */
    {0, 0, 0, 0, 0 } /* terminator */
};
