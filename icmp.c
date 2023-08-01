#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "net.h"

static packet_consumer_t *icmp_consumer;
static timer_t icmp_throttle_timer = 0;

void net_icmp_send_unreachable(packet_t *packet)
{
    if(!timer_expired(icmp_throttle_timer))
        return;

    // rate limit how fast we send ICMP unreachable messages
    icmp_throttle_timer = set_timer_ms(100);

    // TODO check that the destination ip address matches?

    packet_t *unreach = packet_create_icmp(ntohl(packet->ipv4->source_ip), sizeof(ipv4_header_t) + 8);

    // fill in dest MAC (TODO - let the ARP code figure this out?)
    memcpy(unreach->eth->destination_mac, packet->eth->source_mac, sizeof(macaddr_t));
    unreach->flags |= packet_flag_destination_mac_valid;

    // fill in ICMP message
    unreach->icmp->type = 3; // Destination Unreachable
    unreach->icmp->code = 3; // Port Unreachable
    memcpy(unreach->icmp->payload, packet->ipv4, sizeof(ipv4_header_t) + 8);
    net_compute_icmp_checksum(unreach);

    net_tx(unreach);
}

static void net_icmp_pump(packet_consumer_t *c)
{
    packet_t *packet;

    while((packet = packet_queue_pophead(c->queue))){
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
            packet->flags |= packet_flag_destination_mac_valid;

            // recompute checksums
            net_compute_ipv4_checksum(packet);
            net_compute_icmp_checksum(packet);

            net_tx(packet);
            // do not free packet -- transmission side will free it later
        }else{
            // unhandled
            packet_free(packet);
        }
    }
}

void net_icmp_init(void)
{
    icmp_consumer = packet_consumer_alloc();
    icmp_consumer->match_ethertype = ethertype_ipv4;
    icmp_consumer->match_ipv4_protocol = ip_proto_icmp;
    icmp_consumer->queue_pump = net_icmp_pump;
    net_add_packet_consumer(icmp_consumer);
}
