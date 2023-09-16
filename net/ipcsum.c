/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include <timers.h>
#include <cli.h>
#include <net.h>

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
    // printf("write net_verify_tcp_checksum!\n");
    return true;
}

void net_compute_tcp_checksum(packet_t *packet)
{
    printf("write net_compute_tcp_checksum!\n");
}

