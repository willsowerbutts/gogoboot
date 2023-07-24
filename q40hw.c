#include "q40hw.h"

#define MASTER_ADDRESS  0xff000000
#define MASTER_LED      0x30

static volatile unsigned char * const q40_master_led = (volatile unsigned char *)(MASTER_ADDRESS + MASTER_LED);

void q40_led(int on)
{
    *q40_master_led = on ? 0xff : 0;
}
