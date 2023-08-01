#include <q40types.h>
#include <stdlib.h>
#include "tinyalloc.h"
#include "q40hw.h"
#include "cli.h"
#include "net.h"

macaddr_t const macaddr_broadcast = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
macaddr_t macaddr_interface; // MAC address of our interface

static packet_queue_t *net_txqueue;

void net_init(void)
{
    net_txqueue = packet_queue_alloc();
    dhcp_init();
}

void net_pump(void)
{
    eth_pump(); // calls net_eth_push
    dhcp_pump();
    // arp_pump();
}

bool net_process_icmp(packet_t *packet)
{
    if(packet->icmp->type == 8){ // echo request
        // convert echo request to echo reply (per RFC792!)
        packet->icmp->type = 0; // echo reply

        // swap ips
        uint32_t x = packet->ipv4->source_ip;
        packet->ipv4->source_ip = packet->ipv4->destination_ip;
        packet->ipv4->destination_ip = x;

        // swap macs
        macaddr_t m;
        memcpy(m, packet->eth->destination_mac, sizeof(macaddr_t));
        memcpy(packet->eth->destination_mac, packet->eth->source_mac, sizeof(macaddr_t));
        memcpy(packet->eth->source_mac, m, sizeof(macaddr_t));

        // recompute checksums
        net_compute_ipv4_checksum(packet);
        net_compute_icmp_checksum(packet);

        net_tx(packet);
        return true; // transmission side will free it later
    }

    return false; // no thanks, caller can free it
}

// --- receive pipe ---

void net_eth_push(packet_t *packet) // called by ne2000.c
{
    bool taken = false;

    // check that the destination MAC is either our MAC, or a multicast MAC
    if(memcmp(packet->eth->destination_mac, macaddr_interface, sizeof(macaddr_t)) == 0 ||
       packet->eth->destination_mac[0] & 1){ // test multicast bit
        // determine protocol and verify checksums
        switch(packet->eth->ethertype){
            case ethertype_ipv4:
                packet->ipv4 = (ipv4_header_t*)packet->eth->payload;
                if(!net_verify_ipv4_checksum(packet)){
                    printf("rx bad cksum ipv4\n");
                    goto badpkt;
                }
                switch(packet->ipv4->protocol){
                    case ip_proto_tcp:
                        packet->tcp = (tcp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->tcp->options + ((packet->tcp->data_offset & 0x0F) << 2);
                        if(!net_verify_tcp_checksum(packet)){
                            printf("rx bad cksum tcp\n");
                            goto badpkt;
                        }
                        break;
                    case ip_proto_udp:
                        packet->udp = (udp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->udp->payload;
                        if(!net_verify_udp_checksum(packet)){
                            printf("rx bad cksum udp\n");
                            goto badpkt;
                        }
                        break;
                    case ip_proto_icmp:
                        packet->icmp = (icmp_header_t*)packet->ipv4->payload;
                        packet->user_data = packet->icmp->payload;
                        if(!net_verify_icmp_checksum(packet)){
                            printf("rx bad cksum icmp\n");
                            goto badpkt;
                        }
                        taken = net_process_icmp(packet);
                        break;
                    default:
                        // unhandled ipv4 protocol
                        goto badpkt;
                }
                break;
            //case ethertype_arp:
            //    // packet->arp = packet->eth->payload;
            //    // VERIFY CHECKSUM
            //    //...
            //    break;
            default:
                // unhandled ethertype
                goto badpkt;
        }
        // have a list of things to match against -- callback for each
        // "sockets" could have a match specied, a timer, a callback for data, and a callback for timeout?
        // {
        //     match field
        //     match value
        //     timer expiry
        //     callback for timer
        //     callback for data
        //     private pointer for socket
        // }
        /* filter the packet into a queue for the receiving app ... */
        /* generate icmp unreachable if no receiving app ... */
    }

    if(!taken){
badpkt:
        packet_free(packet);
    }
}

// --- transmit pipe ---

void net_tx(packet_t *packet)
{
    // if(!flags & FLAG_DEST_MAC_SET)
    //     packet_queue_addtail(net_txqueue_do_arp, packet);
    // else
    packet_queue_addtail(net_txqueue, packet);
}

packet_t *net_eth_pull(void) // called by ne2000.c
{
    return packet_queue_pophead(net_txqueue);
}
