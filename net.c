/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "timers.h"
#include "cli.h"
#include "net.h"

macaddr_t const broadcast_macaddr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
macaddr_t interface_macaddr; // MAC address of our interface

uint32_t interface_ipv4_address = 0; // IPv4 address of our interface
uint32_t interface_subnet_mask = 0;
uint32_t interface_gateway = 0;
uint32_t interface_dns_server = 0;

static packet_sink_t *net_packet_sink_head = NULL;
static packet_queue_t *net_txqueue = NULL;
static packet_queue_t *net_txqueue_arp_lookup = NULL;

uint32_t packet_alive_count = 0;
uint32_t packet_discard_count = 0;
uint32_t packet_bad_cksum_count = 0;
uint32_t packet_rx_count = 0;
uint32_t packet_tx_count = 0;

void net_init(void)
{
    net_txqueue = packet_queue_alloc();
    net_txqueue_arp_lookup = packet_queue_alloc();
    net_arp_init();
    net_icmp_init();
    dhcp_init();
}

void net_pump(void)
{
    packet_t *packet;

    // pump the hardware driver
    eth_pump(); // calls net_eth_push, net_eth_pull

    // pump each sink with data waiting or an expired timer
    packet_sink_t *sink = net_packet_sink_head;
    while(sink){
        if(sink->cb_packet_received){
            while((packet = packet_queue_pophead(sink->queue)))
                sink->cb_packet_received(sink, packet);
        }
        if(sink->cb_timer_expired && sink->timer && timer_expired(sink->timer)){
            sink->timer = 0; // disable timer
            sink->cb_timer_expired(sink);
        }
        // walk linked list
        sink = sink->next;
    }
}

static int score_sink(packet_sink_t *sink)
{
    int matches = 0;
    if(sink->match_local_ip || sink->match_interface_local_ip)
        matches++;
    if(sink->match_remote_ip)
        matches++;
    if(sink->match_local_port)
        matches++;
    if(sink->match_remote_port)
        matches++;
    if(sink->match_ethertype)
        matches++;
    if(sink->match_ipv4_protocol)
        matches++;
    return matches;
}

void net_add_packet_sink(packet_sink_t *sink)
{
    int score;
    packet_sink_t **ptr;
    packet_sink_t *entry;

    if(sink->next){
        printf("net_add_packet_sink: already in a list?\n");
        return;
    }

    // first sink?
    if(net_packet_sink_head == NULL){
        net_packet_sink_head = sink;
        return;
    }

    // count how many parameters this sink matches against
    score = score_sink(sink);

    // place it in the list in sorted order: we want to test
    // the most specific sinks first
    ptr = &net_packet_sink_head;
    entry = net_packet_sink_head;
    while(entry){
        if(score_sink(entry) <= score){
            // insert
            *ptr = sink;
            sink->next = entry;
            return;
        }
        if(entry->next == NULL){
            entry->next = sink;
            return;
        }
        // walk list
        ptr = &entry->next;
        entry = entry->next;
    }
}

void net_remove_packet_sink(packet_sink_t *sink)
{
    packet_sink_t **ptr;
    packet_sink_t *entry;

    ptr = &net_packet_sink_head;
    entry = net_packet_sink_head;

    while(entry){
        if(entry == sink){
            // we got it!
            *ptr = entry->next;
            entry->next = NULL;
            return;
        }else{
            // walk list
            ptr = &entry->next;
            entry = entry->next;
        }
    }

    printf("net_remove_packet_sink: can't find it?\n");
}

void net_dump_packet_sinks(void) // used by "netinfo" command
{
    packet_sink_t *sink = net_packet_sink_head;
    while(sink){
        printf("sink@0x%lx:\n  ipv4_protocol=0x%x, local_ip=0x%lx, remote_ip=0x%lx, local_port=%d, remote_port=%d\n  ethertype=0x%x, queue_len=%d, packets_queued=%ld, timer=%ld\n",
                (long)sink,
                ntohs(sink->match_ipv4_protocol),
                ntohl(sink->match_local_ip),
                ntohl(sink->match_remote_ip),
                ntohs(sink->match_local_port),
                ntohs(sink->match_remote_port),
                ntohs(sink->match_ethertype),
                packet_queue_length(sink->queue),
                sink->packets_queued,
                sink->timer ? sink->timer - q40_read_timer_ticks() : -1);
        sink = sink->next;
    }

}

// --- receive pipe ---

void net_eth_push(packet_t *packet) // called by ne2000.c
{
    int header_size;

    packet_rx_count++;

    // check that the destination MAC is either our MAC, or a multicast MAC
    if(memcmp(packet->eth->destination_mac, interface_macaddr, sizeof(macaddr_t)) == 0 ||
       packet->eth->destination_mac[0] & 1){ // test multicast bit
        // determine protocol and verify checksums
        switch(ntohs(packet->eth->ethertype)){
            case ethertype_ipv4:
                packet->ipv4 = (ipv4_header_t*)packet->eth->payload;
                if(!net_verify_ipv4_checksum(packet))
                    goto bad_cksum;
                switch(packet->ipv4->protocol){
                    case ip_proto_tcp:
                        header_size = ((packet->tcp->data_offset & 0x0F) << 2);
                        packet->tcp = (tcp_header_t*)packet->ipv4->payload;
                        packet->data = packet->ipv4->payload + header_size;
                        packet->data_length = ntohs(packet->ipv4->length) - header_size;
                        if(!net_verify_tcp_checksum(packet))
                            goto bad_cksum;
                        break;
                    case ip_proto_udp:
                        packet->udp = (udp_header_t*)packet->ipv4->payload;
                        packet->data = packet->udp->payload;
                        packet->data_length = ntohs(packet->udp->length) - sizeof(udp_header_t);
                        if(!net_verify_udp_checksum(packet))
                            goto bad_cksum;
                        break;
                    case ip_proto_icmp:
                        packet->icmp = (icmp_header_t*)packet->ipv4->payload;
                        packet->data = packet->icmp->payload;
                        packet->data_length = ntohs(packet->ipv4->length) - sizeof(icmp_header_t) - sizeof(ipv4_header_t);
                        if(!net_verify_icmp_checksum(packet))
                            goto bad_cksum;
                        break;
                    default:
                        // unhandled ipv4 protocol
                        break;
                }
                break;
            case ethertype_arp:
                packet->arp = (arp_header_t*)packet->eth->payload;
                break;
            default:
                // unhandled ethertype
                break;
        }

        // figure out the best matching queue to put it into
        uint32_t interface_ipv4_address_nbo = htonl(interface_ipv4_address); // convert to network byte order
        packet_sink_t *sink = net_packet_sink_head;
        while(sink){
            if( (sink->match_ethertype == 0      || (sink->match_ethertype == packet->eth->ethertype)) &&
                (sink->match_ipv4_protocol == 0  || (packet->ipv4 && sink->match_ipv4_protocol == packet->ipv4->protocol)) &&
                (sink->match_local_ip == 0       || (packet->ipv4 && sink->match_local_ip == packet->ipv4->destination_ip)) &&
                (!sink->match_interface_local_ip || (packet->ipv4 && interface_ipv4_address_nbo == packet->ipv4->destination_ip)) &&
                (sink->match_remote_ip == 0      || (packet->ipv4 && sink->match_remote_ip == packet->ipv4->source_ip)) &&
                (sink->match_local_port == 0     || ((packet->tcp && sink->match_local_port == packet->tcp->destination_port)) || (packet->udp && sink->match_local_port == packet->udp->destination_port)) &&
                (sink->match_remote_port == 0    || ((packet->tcp && sink->match_remote_port == packet->tcp->source_port) || (packet->udp && sink->match_remote_port == packet->udp->source_port))) ) {
                // enqueue the packet for later processing
                packet_queue_addtail(sink->queue, packet);
                sink->packets_queued++;
                return;
            }
            sink = sink->next;
        }
    }

    // if we didn't find a queue, discard it.
    packet_discard_count++;
    packet_free(packet);
    return;

bad_cksum:
    printf("rx: bad checksum\n");
    packet_bad_cksum_count++;
    packet_free(packet);
}

// --- transmit pipe ---

void net_tx(packet_t *packet)
{
    if(packet->eth->ethertype == ethertype_ipv4){
        net_compute_ipv4_checksum(packet);
        switch(packet->ipv4->protocol){
            case ip_proto_tcp:
                net_compute_tcp_checksum(packet);
                break;
            case ip_proto_udp:
                net_compute_udp_checksum(packet);
                break;
            case ip_proto_icmp:
                net_compute_icmp_checksum(packet);
                break;
        }
    }

    if(packet->flags & packet_flag_destination_mac_valid)
        packet_queue_addtail(net_txqueue, packet);
    else{
        printf("net_txqueue_arp_lookup is a queue to nowhere\n");
        packet_queue_addtail(net_txqueue_arp_lookup, packet);
    }
}

packet_t *net_eth_pull(void) // called by ne2000.c
{
    packet_t *p = packet_queue_pophead(net_txqueue);
    if(p)
        packet_tx_count++;
    return p;
}
