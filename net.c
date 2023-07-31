#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "cli.h"
#include "net.h"

static packet_queue_t *net_rxqueue;
static packet_queue_t *net_txqueue;

void net_init(void)
{
    net_rxqueue = packet_queue_alloc();
    net_txqueue = packet_queue_alloc();
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
    packet_t *p;
    while((p = packet_queue_pophead(q)))
        packet_free(p);
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

packet_t *packet_queue_peekhead(packet_queue_t *q)
{
    return q->head;
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

packet_t *packet_alloc(int data_size)
{
    if(data_size >= PACKET_MAXLEN)
        printf("net: packet_alloc(%d): too big!\n", data_size);
    packet_t *p=malloc(sizeof(packet_t) + data_size);
    p->next = 0;
    p->length = data_size;
    return p;
}

void packet_free(packet_t *packet)
{
    free(packet);
}

void net_rx_processing(packet_t *packet)
{
    // printf("received packet, length %d:\n", packet->length);
    // pretty_dump_memory(packet->data, packet->length);
    /* filter the packet into a queue for the receiving app ... */
    packet_free(packet);
}

void net_pump(void)
{
    packet_t *p;

    eth_pump();

    while(true){
        p = packet_queue_pophead(net_rxqueue);
        if(!p)
            break;
        net_rx_processing(p);
    }

    dhcp_pump();
}

void net_eth_push(packet_t *packet)
{
    packet_queue_addtail(net_rxqueue, packet);
}

packet_t *net_eth_pull(void)
{
    return packet_queue_pophead(net_txqueue);
}

void net_tx(packet_t *packet)
{
    packet_queue_addtail(net_txqueue, packet);
}
