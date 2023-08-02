/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "q40hw.h"
#include "cli.h"
#include "net.h"

static packet_t *packet_create_ipv4(uint32_t dest_ipv4, int data_size, int proto)
{
    packet_t *p = packet_alloc(sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + data_size);

    // set up ethernet header
    memcpy(&p->eth->source_mac, interface_macaddr, sizeof(macaddr_t));
    p->eth->ethertype = htons(ethertype_ipv4);

    // we can skip ARP for broadcast
    if(dest_ipv4 == ipv4_broadcast)
        packet_set_destination_mac(p, &broadcast_macaddr);

    // set up ipv4 header
    p->ipv4 = (ipv4_header_t*)p->eth->payload;
    p->ipv4->version_length = 0x45;   // we don't use options so length = 5 x 4 = 20 bytes
    p->ipv4->diffserv_ecn = 0;
    p->ipv4->length = htons(sizeof(ipv4_header_t) + data_size);
    p->ipv4->id = htons(q40_read_timer_ticks() & 0xffff);
    p->ipv4->flags_and_frags = htons(0x4000); // don't fragment
    p->ipv4->ttl = DEFAULT_TTL;
    p->ipv4->protocol = proto;
    p->ipv4->source_ip = htonl(interface_ipv4_address);
    p->ipv4->destination_ip = htonl(dest_ipv4);

    return p;
}

packet_t *packet_create_icmp(uint32_t dest_ipv4, int data_size)
{
    packet_t *p = packet_create_ipv4(dest_ipv4, data_size + sizeof(icmp_header_t), ip_proto_icmp);
    p->icmp = (icmp_header_t*)p->ipv4->payload;
    p->data = (uint8_t*)p->icmp->payload;
    p->data_length = data_size;
    return p;
}

packet_t *packet_create_tcp(uint32_t dest_ipv4, uint16_t destination_port, uint16_t source_port, int data_size)
{
    packet_t *p = packet_create_ipv4(dest_ipv4, data_size + sizeof(tcp_header_t), ip_proto_udp);
    p->tcp = (tcp_header_t*)p->ipv4->payload;
    p->data = (uint8_t*)p->tcp->options; // + options_size
    p->data_length = data_size;          // - options_size

    printf("packet_create_tcp is incomplete!\n");

    // set up tcp header
    // p->tcp->... = ...;

    // p->data needs to be computed depending on tcp option headers present.

    return p;
}

packet_t *packet_create_udp(uint32_t dest_ipv4, uint16_t destination_port, uint16_t source_port, int data_size)
{
    packet_t *p = packet_create_ipv4(dest_ipv4, data_size + sizeof(udp_header_t), ip_proto_udp);

    // set up udp header
    p->udp = (udp_header_t*)p->ipv4->payload;
    p->data = (uint8_t*)p->udp->payload;
    p->data_length = data_size;

    p->udp->source_port = htons(source_port);
    p->udp->destination_port = htons(destination_port);
    p->udp->length = htons(sizeof(udp_header_t) + data_size);

    return p;
}
