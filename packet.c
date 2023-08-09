/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "timers.h"
#include "cli.h"
#include "net.h"

packet_sink_t *packet_sink_alloc(void)
{
    packet_sink_t *c = malloc(sizeof(packet_sink_t));
    memset(c, 0, sizeof(packet_sink_t));
    c->queue = packet_queue_alloc();
    return c;
}

void packet_sink_free(packet_sink_t *c)
{
    packet_queue_free(c->queue);
    free(c);
}

packet_queue_t *packet_queue_alloc(void)
{
    packet_queue_t *q = malloc(sizeof(packet_queue_t));
    q->head = q->tail = NULL;
    return q;
}

int packet_queue_length(packet_queue_t *q)
{
    int l = 0;
    packet_t *p;

    p = q->head;
    while(p){
        l++;
        p = p->next;
    }

    return l;
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
    if(p->next){
        p->next = NULL;
        printf("packet_queue_addtail: p->next set?\n");
    }
    if(!q)
        printf("null packet_queue?\n");

    if(q->tail){
        q->tail->next = p;
        q->tail = p;
    }else{
        q->head = q->tail = p;
    }
}

packet_t *packet_queue_peekhead(packet_queue_t *q)
{
    // like a pop but we don't remove it from the queue.
    // this can be used to check if the queue is empty 
    // without committing to taking a packet from it.
    if(!q)
        printf("null packet_queue?\n");
    return q->head;
}

packet_t *packet_queue_pophead(packet_queue_t *q)
{
    packet_t *p;

    if(!q)
        printf("null packet_queue?\n");

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

    packet_alive_count++;
    if(packet_alive_count > 10)
        printf("packet_alive_count=%ld\n", packet_alive_count);

    packet_t *p=malloc(sizeof(packet_t) + data_size);
    memset(p, 0, sizeof(packet_t)); // do not zero out the data, just the header
    p->buffer_length_alloc = p->buffer_length = data_size;
    p->eth = (ethernet_header_t*)p->buffer;
    return p;
}

void packet_set_destination_mac(packet_t *packet, const macaddr_t *mac)
{
    memcpy(packet->eth->destination_mac, mac, sizeof(macaddr_t));
    packet->flags |= packet_flag_destination_mac_valid;
}

void packet_free(packet_t *packet)
{
    free(packet);
    packet_alive_count--;
}
