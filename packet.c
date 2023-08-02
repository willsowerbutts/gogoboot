#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "q40hw.h"
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

static packet_t *packet_create_ipv4(uint32_t dest_ipv4, int data_size, int proto)
{
    packet_t *p = packet_alloc(sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + data_size);

    // set up ethernet header
    memcpy(&p->eth->source_mac, net_get_interface_mac(), 6);
    p->eth->ethertype = htons(ethertype_ipv4);
    if(dest_ipv4 == ipv4_broadcast)
        packet_set_destination_mac(p, &macaddr_broadcast);

    // set up ipv4 header
    p->ipv4 = (ipv4_header_t*)p->eth->payload;
    p->ipv4->version_length = 0x45;   // we don't use options so length = 5 x 4 = 20 bytes
    p->ipv4->diffserv_ecn = 0;
    p->ipv4->length = htons(sizeof(ipv4_header_t) + data_size);
    p->ipv4->id = htons(q40_read_timer_ticks() & 0xffff);
    p->ipv4->flags_and_frags = htons(0x4000); // don't fragment
    p->ipv4->ttl = DEFAULT_TTL;
    p->ipv4->protocol = proto;
    p->ipv4->source_ip = htonl(net_get_interface_ipv4());
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

void packet_free(packet_t *packet)
{
    free(packet);
    packet_alive_count--;
}

static uint32_t checksum_update(uint32_t sum, uint16_t *addr, unsigned int count)
{
    // sum words
    while(count >= 2){
        sum += *(addr++);
        count-=2;
    }

    //if any bytes left, pad the bytes and add
    if(count > 0) {
        sum += ((*addr)&htons(0xFF00));
    }

    return sum;
}

static uint16_t checksum_complete(uint32_t sum)
{
    // Fold sum to 16 bits: add carrier to result
    while (sum>>16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    // one's complement
    return ((unsigned short)~sum);
}

static uint16_t checksum_compute(uint16_t *addr, unsigned int count) 
{
    return checksum_complete(checksum_update(0, addr, count));
}

void net_compute_ipv4_checksum(packet_t *packet)
{
    packet->ipv4->checksum = 0; // set to zero for checksum computation
    packet->ipv4->checksum = htons(checksum_compute((uint16_t*)packet->ipv4, 
                sizeof(ipv4_header_t)));
}

bool net_verify_ipv4_checksum(packet_t *packet)
{
    return (checksum_compute((uint16_t*)packet->ipv4, sizeof(ipv4_header_t)) == 0);
}

void net_compute_icmp_checksum(packet_t *packet)
{
    packet->icmp->checksum = 0;
    packet->icmp->checksum = htons(checksum_compute((uint16_t*)packet->icmp,
                ntohs(packet->ipv4->length) - sizeof(ipv4_header_t)));
}

bool net_verify_icmp_checksum(packet_t *packet)
{
    return (checksum_compute((uint16_t*)packet->icmp, ntohs(packet->ipv4->length) - sizeof(ipv4_header_t)) == 0);
}

static uint16_t udp_checksum_pseudoheader(packet_t *packet)
{
    uint32_t sum;
    // we have to sum a "pseudo-header"
    sum = checksum_update(0, (uint16_t*)&packet->ipv4->source_ip, sizeof(uint32_t)*2);
    sum += packet->ipv4->protocol;
    sum += packet->udp->length; // yes, this field is summed twice!
                                // ... then the real udp header + data
    sum = checksum_update(sum, (uint16_t*)packet->udp, ntohs(packet->udp->length));
    return htons(checksum_complete(sum));
}

bool net_verify_udp_checksum(packet_t *packet)
{
    return (packet->udp->checksum == 0 || udp_checksum_pseudoheader(packet) == 0);
}

void net_compute_udp_checksum(packet_t *packet)
{
    uint16_t cs;
    packet->udp->checksum = 0;
    cs = udp_checksum_pseudoheader(packet);
    if(cs == 0) 
        cs = 0xffff; // per RFC768
    packet->udp->checksum = htons(cs);
}

bool net_verify_tcp_checksum(packet_t *packet)
{
    printf("write net_verify_tcp_checksum!\n");
    return true;
}

void net_compute_tcp_checksum(packet_t *packet)
{
    printf("write net_compute_tcp_checksum!\n");
}

