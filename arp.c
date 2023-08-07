/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "timers.h"
#include "tinyalloc.h"
#include "net.h"

#undef ARP_DEBUG

typedef enum {
    ARP_VALID,
    ARP_RESOLVING_TRY1,
    ARP_RESOLVING_TRY2,
    ARP_RESOLVING_TRY3,
    ARP_RESOLVING_TRY4,
} arp_state_t;

#define CACHE_TIMEOUT   60
#define CACHE_FLUSH_INTERVAL 10

static packet_sink_t *sink;

typedef struct arp_cache_entry_t arp_cache_entry_t;

typedef struct arp_cache_entry_t {
    arp_cache_entry_t *next;
    uint32_t ipv4_address;
    macaddr_t mac_address;
    arp_state_t state;
    timer_t next_event;
} arp_cache_entry_t;

arp_cache_entry_t *cache_list_head;

// next_event means:
//   ARP_RESOLVING_*: next resolve attempt time
//   ARP_VALID:       expiry time

static packet_t *packet_create_arp(void)
{
    packet_t *p = packet_alloc(sizeof(ethernet_header_t) + sizeof(arp_header_t));

    // set up ethernet header
    memcpy(&p->eth->source_mac, interface_macaddr, sizeof(macaddr_t));
    p->eth->ethertype = htons(ethertype_arp);

    // set up arp header
    p->arp = (arp_header_t*)p->eth->payload;
    p->arp->hardware_type = htons(1);
    p->arp->protocol_type = htons(0x800);
    p->arp->hardware_length = 6;
    p->arp->protocol_length = 4;
    memcpy(&p->arp->sender_mac, interface_macaddr, sizeof(macaddr_t));
    p->arp->sender_ip = htonl(interface_ipv4_address);

    return p;
}

static void update_arp_cache(uint32_t ip, macaddr_t *mac, bool add_entry)
{
    arp_cache_entry_t *entry = cache_list_head;

    while(entry){
        if(entry->ipv4_address == ip)
            break;
        else
            entry = entry->next;
    }

    if(entry == NULL){
#ifdef ARP_DEBUG
        printf("arp: no match for ip 0x%08lx\n", ip);
#endif
        if(!add_entry)
            return;
#ifdef ARP_DEBUG
        printf("arp: creating entry for ip 0x%08lx\n", ip);
#endif
        entry = malloc(sizeof(arp_cache_entry_t));
        entry->next = cache_list_head;
        cache_list_head = entry;
    }

#ifdef ARP_DEBUG
    printf("arp: updating entry for ip 0x%08lx\n", ip);
#endif
    entry->ipv4_address = ip;
    memcpy(entry->mac_address, mac, sizeof(macaddr_t));
    entry->state = ARP_VALID;
    entry->next_event = set_timer_sec(CACHE_TIMEOUT);
}

static void arp_process_packet(packet_sink_t *sink, packet_t *packet)
{
    bool add_entry = false;

    if(packet->arp->hardware_type   == htons(1) && 
            packet->arp->protocol_type   == htons(0x800) &&
            packet->arp->hardware_length == htons(6) && 
            packet->arp->protocol_length == htons(4)){
        switch(ntohs(packet->arp->operation)){
            case arp_op_request: // who has <ip>?
#ifdef ARP_DEBUG
                printf("arp request:\n");
                pretty_dump_memory(packet->arp, sizeof(arp_header_t));
#endif
                if(interface_ipv4_address && packet->arp->target_ip == htonl(interface_ipv4_address)){
#ifdef ARP_DEBUG
                    printf("arp: replying to ip 0x%08lx\n", ntohl(packet->arp->sender_ip));
#endif
                    // this was for us; generate an ARP reply
                    packet_t *reply = packet_create_arp();
                    reply->arp->operation = arp_op_reply;
                    reply->arp->target_ip = packet->arp->sender_ip;
                    memcpy(reply->arp->target_mac, packet->arp->sender_mac, sizeof(macaddr_t));
                    packet_set_destination_mac(reply, &reply->arp->target_mac);
                    net_tx(reply);
                    // additionally, add the sender is in our ARP cache if not already present
                    add_entry = true;
                }
                // intended fall-through
            case arp_op_reply:
                // for both request and reply we update or ARP cache if the entry exists already
                update_arp_cache(ntohl(packet->arp->sender_ip), &packet->arp->sender_mac, add_entry);
                break;
            default:
#ifdef ARP_DEBUG
                printf("arp: unrecognised operation %d?\n", ntohs(packet->arp->operation));
                pretty_dump_memory(packet->arp, sizeof(arp_header_t));
#endif
                break;
        }
    }
    packet_free(packet);
}

static void arp_cache_flush(packet_sink_t *sink)
{
    int total = 0, expired = 0;

    arp_cache_entry_t *entry = cache_list_head, *next;
    arp_cache_entry_t **entry_ptr = &cache_list_head;

    sink->timer = set_timer_sec(CACHE_FLUSH_INTERVAL);

    while(entry){
        total++;
        next = entry->next; // stash this now, in case we free this entry
        if(entry->state == ARP_VALID){
            if(timer_expired(entry->next_event)){
                // timed out -- remove entry
#ifdef ARP_DEBUG
                printf("arp: flushing entry for ip 0x%08lx\n", entry->ipv4_address);
#endif
                expired++;
                *entry_ptr = entry->next;
                free(entry);
            }
        }
        entry = next;
    }

#ifdef ARP_DEBUG
    printf("arp: flushed cache, %d total, %d expired\n", total, expired);
#endif

}

void net_arp_init(void)
{
    cache_list_head = NULL;
    sink = packet_sink_alloc();
    sink->match_ethertype = htons(ethertype_arp);
    sink->cb_packet_received = arp_process_packet;
    sink->timer = set_timer_sec(CACHE_FLUSH_INTERVAL);
    sink->cb_timer_expired = arp_cache_flush;
    net_add_packet_sink(sink);
}
