#include "q40hw.h"
#include "q40isa.h"
#include "q40uart.h"

void cboot(void)
{
    q40_led(true);
    uart_init();
    uart_write_str("hello, world!\n");
}
