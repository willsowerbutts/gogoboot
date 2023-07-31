#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "q40hw.h"
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
    // like a pop but we don't remove it from the queue.
    // this can be used to check if the queue is empty 
    // without committing to taking a packet from it.
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
    p->length_alloc = p->length = data_size;
    p->eth = (ethernet_header_t*)p->buffer;
    return p;
}

packet_t *packet_create_ipv4(macaddr_t *dest_mac, uint32_t dest_ipv4, int data_size, int proto)
{
    int hsize;

    switch(proto){
        case ip_proto_tcp: hsize = sizeof(tcp_header_t); break;
        case ip_proto_udp: hsize = sizeof(udp_header_t); break;
        // case ip_proto_icmp: size += sizeof(icmp_header_t); break;
        default: printf("proto=%d?", proto); return NULL;
    }

    packet_t *p = packet_alloc(sizeof(ethernet_header_t) +
                               sizeof(ipv4_header_t) +
                               hsize + data_size);

    // set up ethernet header
    memcpy(&p->eth->destination_mac, dest_mac, 6);
    memcpy(&p->eth->source_mac, eth_get_interface_mac(), 6);
    p->eth->ethertype = htons(ethertype_ipv4);

    // set up ipv4 header
    p->ipv4 = (ipv4_header_t*)p->eth->payload;
    p->ipv4->version_length = 0x45;   // we don't use options so length = 5 x 4 = 20 bytes
    p->ipv4->diffserv_ecn = 0;
    p->ipv4->length = htons(sizeof(ipv4_header_t) + hsize + data_size);
    p->ipv4->id = htons(q40_read_timer_ticks() & 0xffff);
    p->ipv4->flags_and_frags = htons(0x4000); // don't fragment
    p->ipv4->ttl = DEFAULT_TTL;
    p->ipv4->protocol = proto;
    p->ipv4->source_ip = 0; // TODO ? net_get_interface_ipv4();
    p->ipv4->destination_ip = htonl(dest_ipv4);
    net_compute_ipv4_checksum(p->ipv4);

    switch(proto){
        case ip_proto_tcp:
            p->tcp = (tcp_header_t*)p->ipv4->payload;
            break;
        case ip_proto_udp:
            p->udp = (udp_header_t*)p->ipv4->payload;
            break;
    }

    return p;
}

packet_t *packet_create_tcp(macaddr_t *dest_mac, uint32_t dest_ipv4, int data_size, 
        uint16_t source_port, uint16_t destination_port)
{
    packet_t *p = packet_create_ipv4(dest_mac, dest_ipv4, data_size, ip_proto_udp);

    // set up tcp header
    // p->tcp->... = ...;

    // p->user_data needs to be computed depending on tcp option headers present.

    return p;
}

packet_t *packet_create_udp(macaddr_t *dest_mac, uint32_t dest_ipv4, int data_size, 
        uint16_t source_port, uint16_t destination_port)
{
    packet_t *p = packet_create_ipv4(dest_mac, dest_ipv4, data_size, ip_proto_udp);

    // set up udp header
    p->udp->source_port = htons(source_port);
    p->udp->destination_port = htons(destination_port);
    p->udp->length = htons(sizeof(udp_header_t) + data_size);
    p->udp->checksum = 0; // udp checksum is optional, skip it for now

    p->user_data = (uint8_t*)p->udp->payload;

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

static uint16_t compute_checksum(uint16_t *addr, unsigned int count) 
{
    uint32_t sum = 0;

    while (count > 1) {
        sum += * addr++;
        count -= 2;
    }

    //if any bytes left, pad the bytes and add
    if(count > 0) {
        sum += ((*addr)&htons(0xFF00));
    }

    //Fold sum to 16 bits: add carrier to result
    while (sum>>16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    //one's complement
    sum = ~sum;
    return ((unsigned short)sum);
}


void net_compute_ipv4_checksum(ipv4_header_t *ipv4)
{
    ipv4->checksum = 0; // set to zero for checksum computation
    ipv4->checksum = htons(compute_checksum((uint16_t*)ipv4, sizeof(ipv4_header_t)));
}
