#include "q40hw.h"
#include "q40isa.h"
#include "q40uart.h"

extern const char copyright_msg[];

void cboot(void)
{
    q40_led(true);
    uart_init();
    uart_write_str("123 "); /* WEIRDNESS: does not work without this? some sort of caching issue, perhaps? */
    uart_write_str(copyright_msg);
    q40_led(false);
}
