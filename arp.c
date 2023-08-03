/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "timers.h"
#include "net.h"

#define FLAG_VALID      (1<<1)
#define FLAG_RESOLVING  (1<<2)
#define CACHE_SIZE      32

static packet_sink_t *sink;

typedef struct arp_cache_entry_t {
    uint32_t ipv4_address;
    macaddr_t mac_address;
    timer_t last_event; // ?
    int16_t flags;
} arp_cache_entry_t;

static arp_cache_entry_t cache[CACHE_SIZE];

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

static void arp_pump(packet_sink_t *sink, packet_t *packet)
{
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

void net_arp_init(void)
{
    memset(cache, 0, sizeof(cache));
    sink = packet_sink_alloc();
    sink->match_ethertype = htons(ethertype_arp);
    sink->cb_packet_received = arp_pump;
    net_add_packet_sink(sink);
}
