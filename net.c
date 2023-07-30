#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "cli.h"
#include "net.h"

static packet_queue_t *net_rxqueue;
static bool net_initialised = false;

void net_init(void)
{
    net_initialised = true;
    net_rxqueue = packet_queue_alloc();
    dhcp_init();
}

packet_queue_t *packet_queue_alloc(void)
{
    packet_queue_t *q = malloc(sizeof(packet_queue_t));
    q->head = q->tail = NULL;
    return q;
}

void packet_queue_free(packet_queue_t *q)
{
    free(q);
}

void packet_queue_addtail(packet_queue_t *q, packet_t *p)
{
    p->next = NULL;

    if(q->tail){
        q->tail->next = p;
        q->tail = p;
    }else{
        q->head = q->tail = p;
    }
}

packet_t *packet_queue_pophead(packet_queue_t *q)
{
    packet_t *p;

    if(!q->head)
        return NULL;

    p = q->head;
    q->head = p->next;
    if(!q->head)
        q->tail = NULL;

    p->next = 0;
    return p;
}

packet_t *net_alloc(int data_size)
{
    return malloc(sizeof(packet_t) + data_size);
}

void net_free(packet_t *packet)
{
    free(packet);
}

void net_rx_processing(packet_t *packet)
{
    printf("received packet, length %d:\n", packet->length);
    pretty_dump_memory(packet->data, packet->length);
    net_free(packet);
    /* filter the packet into a queue for the receiving app ... */
}

void net_pump(void)
{
    if(!net_initialised)
        return;

    eth_pump();

    packet_t *p;
    while((p = packet_queue_pophead(net_rxqueue))){
        net_rx_processing(p);
    }

    dhcp_pump();
}

void net_rx(packet_t *packet)
{
    packet_queue_addtail(net_rxqueue, packet);
}
