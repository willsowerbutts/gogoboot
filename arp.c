/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <types.h>
#include <stdlib.h>
#include "timers.h"
#include "tinyalloc.h"
#include "net.h"

#undef ARP_DEBUG

#define CACHE_TIMEOUT   60
#define CACHE_FLUSH_INTERVAL 15
#define MAX_RESOLVE_ATTEMPTS 10
#define QUERY_INTERVAL 500

static packet_sink_t *sink;

typedef struct arp_cache_entry_t arp_cache_entry_t;

typedef struct arp_cache_entry_t {
    arp_cache_entry_t *next;
    uint32_t ipv4_address;
    macaddr_t mac_address;
    bool valid;
    int resolve_attempts;
    timer_t next_event;
} arp_cache_entry_t;

static arp_cache_entry_t *cache_list_head;

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
    entry->valid = true;
    entry->resolve_attempts = 0;
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
#ifdef ARP_DEBUG
                printf("arp reply:\n");
                pretty_dump_memory(packet->arp, sizeof(arp_header_t));
#endif
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
        if((entry->valid && timer_expired(entry->next_event)) || // valid but timed out
           (!entry->valid && entry->resolve_attempts >= MAX_RESOLVE_ATTEMPTS)){ // resolve failed
            // dead -- remove entry
#ifdef ARP_DEBUG
            printf("arp: flushing %svalid entry for ip 0x%08lx\n", 
                    entry->valid ? "":"in", entry->ipv4_address);
#endif
            expired++;
            next = entry->next; // stash this now, in case we free this entry
            *entry_ptr = entry->next;
            free(entry);
            entry = next;
            // do not update entry_ptr
        }else{
            // walk list
            entry_ptr = &entry->next;
            entry = entry->next;
        }
    }

#ifdef ARP_DEBUG
    printf("arp: flushed cache, %d total, %d expired\n", total, expired);
#endif
}

static void arp_transmit_query(arp_cache_entry_t *entry)
{
    entry->resolve_attempts++;
    entry->next_event = set_timer_ms(QUERY_INTERVAL);

    packet_t *query = packet_create_arp();
    query->arp->operation = arp_op_request;
    query->arp->target_ip = htonl(entry->ipv4_address);
    memset(query->arp->target_mac, 0, sizeof(macaddr_t));
    packet_set_destination_mac(query, &broadcast_macaddr);
#ifdef ARP_DEBUG
    printf("arp transmit query (attempt %d):\n", entry->resolve_attempts);
    pretty_dump_memory(query->arp, sizeof(arp_header_t));
#endif
    net_tx(query);
}

arp_result_t net_arp_resolve(packet_t *packet)
{
    uint32_t target = ntohl(packet->ipv4->destination_ip);
    arp_cache_entry_t *entry = cache_list_head;

    while(entry){
        if(entry->ipv4_address == target)
            break;
        entry = entry->next;
    }

    if(!entry){
        // allocate a new entry and begin resolution
        entry = malloc(sizeof(arp_cache_entry_t));
        memset(entry->mac_address, 0, sizeof(macaddr_t));
        entry->next = cache_list_head;
        cache_list_head = entry;
        entry->ipv4_address = target;
        entry->valid = false;
        entry->resolve_attempts = 0;
        arp_transmit_query(entry);
        return arp_wait;
    }

    if(entry->valid){
        packet_set_destination_mac(packet, &entry->mac_address);
        return arp_okay;
    }

    if(timer_expired(entry->next_event)){
        if(entry->resolve_attempts >= MAX_RESOLVE_ATTEMPTS){
            return arp_fail;
        }
        arp_transmit_query(entry);
    }

    return arp_wait;
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
