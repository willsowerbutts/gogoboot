#ifndef __NET_DOT_H__
#define __NET_DOT_H__

#include <q40types.h>

#define PACKET_BUFFER_SIZE 1600

/* ethernet -- startup and shutdown */
bool eth_init(void); // returns true if card found
void eth_halt(void);

/* ethernet -- poll hardware and send one packet */
bool eth_tx(uint8_t *packet, int length); /* returns true on success */

/* ethernet -- poll hardware and receive one packet (if any ready) 
 * note contents is valid ONLY until eth_tx() or eth_rx next called */
uint8_t *eth_rx(int *length);             /* returns NULL if no packet waiting */

#endif
