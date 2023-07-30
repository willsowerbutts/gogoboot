#ifndef __NET_DOT_H__
#define __NET_DOT_H__

#include <q40types.h>
#include <stdbool.h>

typedef struct packet_t packet_t;

struct packet_t {
    packet_t *next;
    uint16_t length; // length of data[]
    uint8_t data[];  // must be final member of data structure
    // do NOT add more members here
};

/* ne2000.c */
bool eth_init(void); // returns true if card found
void eth_halt(void);
void eth_pump(void);
bool eth_tx(uint8_t *packet, int length); /* returns true on success */

/* net.c */
void net_init(void);
void net_pump(void);
void net_rx(packet_t *packet);
packet_t *net_alloc(int data_size);
void net_free(packet_t *packet);

/* dhcp.c */
void dhcp_init(void);
void dhcp_pump(void);

#endif
