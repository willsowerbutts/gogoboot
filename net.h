#ifndef __NET_DOT_H__
#define __NET_DOT_H__

#include <q40types.h>
#include <stdbool.h>

typedef struct packet_t packet_t;

struct packet_t {
    packet_t *next;  // used by packet_queue_t to create linked lists
    uint16_t length; // length of data[]
    uint8_t data[];  // must be final member of data structure
    // do NOT add more members here
};

#define PACKET_MAXLEN 1600      /* largest size we will process */

typedef struct {
    packet_t *head;
    packet_t *tail;
} packet_queue_t;

/* ne2000.c */
bool eth_init(void); // returns true if card found
void eth_halt(void);
void eth_pump(void); // called from net_pump

/* net.c -- interface with ne2000.c */
void net_eth_push(packet_t *packet);
packet_t *net_eth_pull(void);

/* net.c */
void net_init(void);
void net_pump(void);
void net_tx(packet_t *packet);
packet_t *packet_alloc(int data_size);
void packet_free(packet_t *packet);
packet_queue_t *packet_queue_alloc(void);
void packet_queue_free(packet_queue_t *q);
void packet_queue_addtail(packet_queue_t *q, packet_t *p);
packet_t *packet_queue_peekhead(packet_queue_t *q);
packet_t *packet_queue_pophead(packet_queue_t *q);

/* dhcp.c */
void dhcp_init(void);
void dhcp_pump(void);

#endif
