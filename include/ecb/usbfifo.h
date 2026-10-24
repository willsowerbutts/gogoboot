#ifndef __ECB_USBFIFO_DOT_H__
#define __ECB_USBFIFO_DOT_H__

#define USBFIFO_ADDR            0x0C
#define USBFIFO_DATA            (USBFIFO_ADDR + 0)
#define USBFIFO_STATUS          (USBFIFO_ADDR + 1)

/* bits in USBFIFO_STATUS register */
#define USBFIFO_TX_FULL         0x01
#define USBFIFO_INT_ENABLE      0x02
#define USBFIFO_INT_TX          0x04
#define USBFIFO_INT_RX          0x08
#define USBFIFO_IRQ             0x40
#define USBFIFO_RX_EMPTY        0x80

void do_rxfifo(char *argv[], int argc);

#endif
