/* 2026-10-24 William R Sowerbutts */

/* Receive a file through the ECB USB-FIFO card.
 *
 * Wire format: 32-bit big-endian length, data, 32-bit big-endian CRC32 of
 * the data (the zlib/IEEE 802.3 CRC, as computed by Python's zlib.crc32).
 * See tools/sendfifo. */

#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <fatfs/ff.h>
#include <ecb/ecb.h>
#include <ecb/usbfifo.h>
#include <cli.h>

static uint32_t crc32_table[256];

static void crc32_init(void)
{
    uint32_t c;

    for(int n=0; n<256; n++){
        c = n;
        for(int k=0; k<8; k++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc32_table[n] = c;
    }
}

/* c is the CRC so far, starting from 0 */
static uint32_t crc32(uint32_t c, const uint8_t *p, uint32_t len)
{
    if(!crc32_table[1])
        crc32_init();

    c ^= 0xFFFFFFFF;
    while(len--)
        c = crc32_table[(c ^ *(p++)) & 0xFF] ^ (c >> 8);

    return c ^ 0xFFFFFFFF;
}

/* returns false if cancelled from the console */
static bool usbfifo_read(void *buffer, uint32_t count)
{
    uint8_t *p = buffer;
    uint8_t *end = p + count;

    while(p < end){
        while(ecb_read_byte(USBFIFO_STATUS) & USBFIFO_RX_EMPTY)
            if(uart_check_cancel_key())
                return false;
        *(p++) = ecb_read_byte(USBFIFO_DATA);
    }

    return true;
}

#define RXFIFO_BLOCK (256*1024)

void do_rxfifo(char *argv[], int argc)
{
    FRESULT fr;
    uint8_t *buffer;
    uint32_t count, remain, block, crc, crc_rx;
    UINT written;
    FIL fd;

    buffer = malloc_unchecked(RXFIFO_BLOCK);
    if(buffer == NULL){
        printf("rxfifo: out of memory\n");
        return;
    }

    fr = f_open(&fd, argv[0], FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK){
        printf("rxfifo: cannot create \"%s\": ", argv[0]);
        f_perror(fr);
        free(buffer);
        return;
    }

    ecb_write_byte(USBFIFO_STATUS, 0); /* interrupts off */

    /* stale bytes would be taken as the length */
    while(!(ecb_read_byte(USBFIFO_STATUS) & USBFIFO_RX_EMPTY))
        ecb_read_byte(USBFIFO_DATA);

    printf("rxfifo: loading from USB-FIFO (Q to cancel) ...\n");
    if(!usbfifo_read(&count, sizeof(count)))
        goto cancelled;

    crc = 0;
    for(remain = count; remain; remain -= block){
        block = remain < RXFIFO_BLOCK ? remain : RXFIFO_BLOCK;
        if(!usbfifo_read(buffer, block))
            goto cancelled;
        crc = crc32(crc, buffer, block);
        fr = f_write(&fd, buffer, block, &written);
        if(fr == FR_OK && written != block)
            fr = FR_DENIED; /* disk full */
        if(fr != FR_OK){
            printf("rxfifo: failed to write \"%s\": ", argv[0]);
            f_perror(fr);
            goto discard;
        }
    }

    if(!usbfifo_read(&crc_rx, sizeof(crc_rx)))
        goto cancelled;

    if(crc != crc_rx){
        printf("rxfifo: CRC mismatch (received %08lx, computed %08lx), not saved\n", crc_rx, crc);
        goto discard;
    }

    fr = f_close(&fd);
    free(buffer);
    if(fr != FR_OK){
        printf("rxfifo: failed to write \"%s\": ", argv[0]);
        f_perror(fr);
        return;
    }
    printf("rxfifo: loaded %ld bytes, CRC %08lx OK\n", count, crc);
    return;

cancelled:
    printf("rxfifo: cancelled\n");
discard:
    f_close(&fd);
    f_unlink(argv[0]);
    free(buffer);
}
