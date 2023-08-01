#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "q40hw.h"
#include "cli.h"
#include "net.h"

macaddr_t const macaddr_broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
macaddr_t macaddr_interface; // MAC address of our interface
uint32_t ipv4_addr_interface = 0; // IPv4 address of our interface

static packet_consumer_t *net_packet_consumer_head = NULL;
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
    net_icmp_init();
    dhcp_init();
}

void net_pump(void)
{
    eth_pump(); // calls net_eth_push
    dhcp_pump();
    // arp_pump(); <-- process net_txqueue_arp_lookup in here
}

void net_add_packet_consumer(packet_consumer_t *c)
{
    if(c->next)
        printf("net_add_packet_consumer: already in a list?\n");
    c->next = net_packet_consumer_head;
    net_packet_consumer_head = c;
}

void net_remove_packet_consumer(packet_consumer_t *c)
{
    packet_consumer_t **ptr;
    packet_consumer_t *entry;

    ptr = &net_packet_consumer_head;
    entry = net_packet_consumer_head;

    while(entry){
        if(entry == c){ 
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

    printf("net_remove_packet_consumer: can't find it?\n");
}

// --- receive pipe ---

void net_eth_push(packet_t *packet) // called by ne2000.c
{
    bool taken = false;

    // here we check that we have a packet intended for our MAC address, 
    // then we set up our pointers to the various headers, verify the 
    // checksums, and figure out which queue to put it into. If we cannot
    // find a queue for it, we discard it.
    packet_rx_count++;

    // check that the destination MAC is either our MAC, or a multicast MAC
    if(memcmp(packet->eth->destination_mac, macaddr_interface, sizeof(macaddr_t)) == 0 ||
       packet->eth->destination_mac[0] & 1){ // test multicast bit
        // determine protocol and verify checksums
        switch(ntohs(packet->eth->ethertype)){
            case ethertype_ipv4:
                packet->ipv4 = (ipv4_header_t*)packet->eth->payload;
                if(!net_verify_ipv4_checksum(packet))
                    goto bad_cksum;
                switch(packet->ipv4->protocol){
                    case ip_proto_tcp:
                        packet->tcp = (tcp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->tcp->options + ((packet->tcp->data_offset & 0x0F) << 2);
                        if(!net_verify_tcp_checksum(packet))
                            goto bad_cksum;
                        break;
                    case ip_proto_udp:
                        packet->udp = (udp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->udp->payload;
                        if(!net_verify_udp_checksum(packet))
                            goto bad_cksum;
                        break;
                    case ip_proto_icmp:
                        packet->icmp = (icmp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->icmp->payload;
                        if(!net_verify_icmp_checksum(packet))
                            goto bad_cksum;
                        break;
                    default:
                        // unhandled ipv4 protocol
                        break;
                }
                break;
            //case ethertype_arp:
            //    // packet->arp = packet->eth->payload;
            //    // VERIFY CHECKSUM
            //    //...
            //    break;
            default:
                // unhandled ethertype
                break;
        }

        packet_consumer_t *consumer = net_packet_consumer_head;
        while(consumer && !taken){
            if( (consumer->match_ethertype == 0     || (consumer->match_ethertype == ntohs(packet->eth->ethertype))) &&
                (consumer->match_ipv4_protocol == 0 || (packet->ipv4 && consumer->match_ipv4_protocol == packet->ipv4->protocol)) &&
                (consumer->match_local_ip == 0      || (packet->ipv4 && consumer->match_local_ip == ntohl(packet->ipv4->destination_ip))) &&
                (consumer->match_remote_ip == 0     || (packet->ipv4 && consumer->match_remote_ip == ntohl(packet->ipv4->source_ip))) &&
                (consumer->match_local_port == 0    || (packet->tcp && consumer->match_local_port == ntohs(packet->tcp->destination_port)) || (packet->udp && consumer->match_local_port == ntohs(packet->udp->destination_port))) &&
                (consumer->match_remote_port == 0   || (packet->tcp && consumer->match_remote_port == ntohs(packet->tcp->source_port)) || (packet->udp && consumer->match_remote_port == ntohs(packet->udp->source_port))) ){
                packet_queue_addtail(consumer->queue, packet);
                taken = true;
                break;
            }else{
                consumer = consumer->next;
            }
        }

        if(!taken && packet->ipv4){
            net_icmp_send_unreachable(packet);
        }
    }

    if(!taken){
        packet_discard_count++;
        packet_free(packet);
    }
    return;

bad_cksum:
    printf("rx: bad checksum\n");
    packet_bad_cksum_count++;
    packet_free(packet);
}

// --- transmit pipe ---

void net_tx(packet_t *packet)
{
    if(packet->flags & packet_flag_destination_mac_valid)
        packet_queue_addtail(net_txqueue, packet);
    else{
        printf("net_txqueue_arp_lookup is a queue to nowhere\n");
        packet_queue_addtail(net_txqueue_arp_lookup, packet);
    }
}

packet_t *net_eth_pull(void) // called by ne2000.c
{
    packet_tx_count++;
    return packet_queue_pophead(net_txqueue);
}

const macaddr_t *net_get_interface_mac(void)
{
    return &macaddr_interface;
}

uint32_t net_get_interface_ipv4(void)
{
    return ipv4_addr_interface;
}
