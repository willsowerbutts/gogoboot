#include "q40hw.h"

#define MASTER_ADDRESS  0xff000000
#define MASTER_LED      0x30

void q40_led(bool on)
{
    volatile unsigned char *q40_led = (volatile unsigned char *)(MASTER_ADDRESS + MASTER_LED);
    *q40_led = on ? 1 : 0;
}
