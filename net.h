#ifndef __NET_DOT_H__
#define __NET_DOT_H__

int eth_init(void);
void eth_halt(void);
int eth_rx(void);
int eth_send(void *packet, int length, unsigned long key);

#endif
