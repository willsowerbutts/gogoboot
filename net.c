#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "cli.h"
#include "net.h"

packet_t *rxqueue_head, *rxqueue_tail;

void net_init(void)
{
    rxqueue_head = NULL;
    rxqueue_tail = NULL;
    dhcp_init();
}

packet_t *net_alloc(int data_size)
{
    return (packet_t*)malloc(sizeof(packet_t) + data_size);
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
}

void net_pump(void)
{
    packet_t *rxqueue_next;

    eth_pump();

    // process rxqueue until it is empty
    while(rxqueue_head){
        rxqueue_next = rxqueue_head->next;
        rxqueue_head->next = 0;
        net_rx_processing(rxqueue_head);
        rxqueue_head = rxqueue_next;
    }
    rxqueue_tail = NULL;

    dhcp_pump();
}

void net_rx(packet_t *packet)
{
    // add packet to rxqueue
    packet->next = NULL;
    if(rxqueue_tail){
        rxqueue_tail->next = packet;
        rxqueue_tail = packet;
    }else{
        rxqueue_head = rxqueue_tail = packet;
    }
}
