/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "net.h"

static packet_sink_t *sink;

static packet_t *packet_create_arp(void)
{
    packet_t *p = packet_alloc(sizeof(ethernet_header_t) + sizeof(arp_header_t));

    // set up ethernet header
    memcpy(&p->eth->source_mac, interface_macaddr, sizeof(macaddr_t));
    p->eth->ethertype = htons(ethertype_arp);

    // set up arp header
    p->arp = (arp_header_t*)p->eth->payload;
    p->arp->hardware_type = htons(1);
    p->arp->protocol_type = htons(1);
    p->arp->hardware_length = 6;
    p->arp->protocol_length = 4;
    memcpy(&p->arp->sender_mac, interface_macaddr, sizeof(macaddr_t));
    p->arp->sender_ip = htonl(interface_ipv4_address);

    return p;
}

static void arp_pump(packet_sink_t *sink)
{
    packet_t *packet;

    while((packet = packet_queue_pophead(sink->queue))){
        if(packet->arp->hardware_type   == htons(1) && 
           packet->arp->protocol_type   == htons(0x800) &&
           packet->arp->hardware_length == htons(6) && 
           packet->arp->protocol_length == htons(4)){
           switch(ntohs(packet->arp->operation)){
               case arp_op_request: // who has <ip>?
                   printf("arp request:\n");
                   pretty_dump_memory(packet->arp, sizeof(arp_header_t));
                   break;
               case arp_op_reply:   // <ip> is at <mac>
                   printf("arp reply:\n");
                   pretty_dump_memory(packet->arp, sizeof(arp_header_t));
                   packet_free(packet_create_arp());
                   break;
               default:
                   break;
           }
        }
        packet_free(packet);
    }
}

void net_arp_init(void)
{
    sink = packet_sink_alloc();
    sink->match_ethertype = htons(ethertype_arp);
    sink->cb_queue_pump = arp_pump;
    net_add_packet_sink(sink);
}
