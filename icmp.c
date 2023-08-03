/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "timers.h"
#include "net.h"

static packet_sink_t *icmp_sink;
static packet_sink_t *unwanted_ipv4_sink;
static timer_t icmp_throttle_timer = 0;

static void icmp_send_unreachable(packet_sink_t *sink, packet_t *packet)
{
    if(!timer_expired(icmp_throttle_timer))
        return;

    // rate limit how fast we send ICMP unreachable messages
    icmp_throttle_timer = set_timer_ms(100);

    packet_t *unreach = packet_create_icmp(ntohl(packet->ipv4->source_ip), sizeof(ipv4_header_t) + 8);

    // fill in dest MAC (TODO - let the ARP code figure this out?)
    memcpy(unreach->eth->destination_mac, packet->eth->source_mac, sizeof(macaddr_t));
    unreach->flags |= packet_flag_destination_mac_valid;

    // fill in ICMP message
    unreach->icmp->type = 3; // Destination Unreachable
    unreach->icmp->code = 3; // Port Unreachable
    memcpy(unreach->icmp->payload, packet->ipv4, sizeof(ipv4_header_t) + 8);

    packet_free(packet);
    net_tx(unreach);
}

static void icmp_received(packet_sink_t *sink, packet_t *packet)
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
        packet->flags |= packet_flag_destination_mac_valid;

        net_tx(packet);
        // do not free packet -- transmission side will free it later
    }else{
        // unhandled
        packet_free(packet);
    }
}

void net_icmp_init(void)
{
    // this sink deals with ICMP traffic targetted at us
    icmp_sink = packet_sink_alloc();
    icmp_sink->match_interface_local_ip = true;
    icmp_sink->match_ethertype = htons(ethertype_ipv4);
    icmp_sink->match_ipv4_protocol = htons(ip_proto_icmp);
    icmp_sink->cb_packet_received = icmp_received;
    net_add_packet_sink(icmp_sink);

    // this sink aims to deal with IPv4 traffic that no other sink consumed
    unwanted_ipv4_sink = packet_sink_alloc();
    unwanted_ipv4_sink->match_interface_local_ip = true;
    unwanted_ipv4_sink->match_ethertype = htons(ethertype_ipv4);
    unwanted_ipv4_sink->cb_packet_received = icmp_send_unreachable;
    net_add_packet_sink(unwanted_ipv4_sink);
}
