#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "cli.h"
#include "net.h"
#include "dhcp_internals.h"

int select_wait_time = short_wait_time;

static uint8_t dhcp_server_id[4];
static uint32_t dhcp_offer_ipv4_address;
static uint32_t dhcp_offer_subnet_mask;
static uint32_t dhcp_offer_gateway;
static uint32_t dhcp_offer_dns_server;
static uint32_t dhcp_offer_lease_time;

static dhcp_state_t dhcp_state;
static packet_sink_t *sink;

/* see https://www.rfc-editor.org/rfc/rfc1533 in particular section 9 */
uint8_t const discover_options[] = {
    // DHCP message type - DHCPDISCOVER 
    dhcp_opt_message_type,  0x01, dhcp_type_discover,
    // DHCP parameter request list (1=subnet mask, 3=router, 6=nameserver, 15=domain name)
    dhcp_opt_param_request, 0x04, 0x01, 0x03, 0x06, 0x0f,
};

uint8_t const dhcp_always_options[] = {
    // maximum DHCP message size - 1400 bytes (0x0578)
    dhcp_opt_max_size,      0x02, 0x05, 0x78,
};

static packet_t *packet_create_dhcp(uint32_t target_ipv4, uint8_t message_type, 
        const uint8_t *extra_options, int extra_options_len)
{
    int options_len;

    options_len  = 4; // magic cookie 
    options_len += 3; // dhcp message type
    options_len += sizeof(dhcp_always_options);
    options_len += extra_options_len;
    options_len += 1; // terminator

    packet_t *p = packet_create_udp(target_ipv4, 67, 68,
            sizeof(dhcp_message_t) + options_len);

    dhcp_message_t *d = (dhcp_message_t*)p->data;

    d->op = 1;          // BOOTREQUEST
    d->htype = 1;       // 10M ethernet
    d->hlen = 6;        // ethernet
    d->hops = 0;
    d->xid = q40_read_timer_ticks(); // no need to byte swap since it's random anyway
    d->secs = ntohs(0);        // we can do better here, surely?
    d->flags = ntohs(0);
    d->ciaddr = d->yiaddr = d->siaddr = d->giaddr = htonl(0);
    memcpy(d->chaddr, net_get_interface_mac(), 6);
    memset(d->chaddr+6, 0, 10 + 64 + 128); // zero residue of chaddr, plus sname and file fields together
    
    // write options
    int offset = 0;
    memcpy(d->options+offset, magic_cookie_value, 4);
    offset += 4;
    d->options[offset++] = dhcp_opt_message_type;
    d->options[offset++] = 1; // length
    d->options[offset++] = message_type;
    memcpy(d->options+offset, dhcp_always_options, sizeof(dhcp_always_options));
    offset += sizeof(dhcp_always_options);
    memcpy(d->options+offset, extra_options, extra_options_len);
    offset += extra_options_len;
    d->options[offset++] = dhcp_opt_terminator;

    return p;
}

bool process_dhcp_reply(packet_t *packet, uint8_t expected_dhcp_type)
{
    int offset, length;
    uint8_t opt_code, opt_len, *opt_data;
    dhcp_message_t *d = (dhcp_message_t*)packet->data;

    printf("process_dhcp_reply\n");

    if(d->op != 2) // check for BOOTREPLY
        return false;

    // parse DHCP options
    offset = 4; // skip over magic cookie
    length = packet->data_length - sizeof(dhcp_message_t);

    // check for magic cookie and some options beyond
    if(length <= 4 || 
            d->options[0] != 0x63 ||
            d->options[1] != 0x82 ||
            d->options[2] != 0x53 ||
            d->options[3] != 0x63)
        return false;

    // reset offer state
    dhcp_server_id[0] = dhcp_server_id[1] = dhcp_server_id[2] = dhcp_server_id[3] = 0;
    dhcp_offer_ipv4_address = 0;
    dhcp_offer_subnet_mask = 0;
    dhcp_offer_gateway = 0;
    dhcp_offer_dns_server = 0;
    dhcp_offer_ipv4_address = ntohl(d->yiaddr);

    // process options
    while(offset < length){
        if(d->options[offset] == dhcp_opt_terminator) // END option
            break;
        if(d->options[offset] == dhcp_opt_pad){ // PAD option
            offset++;
            continue;
        }

        opt_code = d->options[offset];
        opt_len = d->options[offset+1];
        opt_data = d->options+offset+2;

        if((length - offset) >= (2 + opt_len)){
            switch(opt_code){
                case dhcp_opt_message_type:
                    if(opt_len != 1 || opt_data[0] != expected_dhcp_type)
                        return false; // we can't use this packet
                    break;
                case dhcp_opt_server_id:
                    if(opt_len != 4)
                        return false;
                    for(int i=0; i<4; i++)
                        dhcp_server_id[i] = opt_data[i];
                    break;
                case dhcp_opt_subnet_mask:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_subnet_mask = htonl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_gateway:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_gateway = htonl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_lease_time:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_lease_time = htonl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_dns_server:
                    if(opt_len < 4)
                        return false;
                    dhcp_offer_dns_server = htonl(*((uint32_t*)opt_data));
                    break;
                default:
                    printf("dhcp: ignored option %02x len %02x:", opt_code, opt_len);
                    for(int i=0; i<opt_len; i++)
                        printf(" %02x", opt_data[i]);
                    printf("\n");
                    break;
            }
        }
        offset += 2 + opt_len;
    }

    return true;
}

static void dhcp_enter_state(dhcp_state_t state)
{
    dhcp_state = state;

    printf("dhcp_enter_state(%d)\n", state);

    switch(dhcp_state){
        case DHCP_DISCOVER:
            // create and send a DHCPDISCOVER message
            net_tx(packet_create_dhcp(ipv4_broadcast, dhcp_type_discover,
                        discover_options, sizeof(discover_options)));
            printf("dhcp_enter_state: packet_create_dhcp + net_tx done\n");
            dhcp_enter_state(DHCP_SELECT);
            break;
        case DHCP_SELECT:
            // start a timer for DHCPOFFER
            sink->timer = set_timer_sec(select_wait_time);
            // increase the time we wait in future cycles, up to ~2 minutes
            // this is reset when we send a DHCPREQUEST
            if(select_wait_time < 120)
                select_wait_time = select_wait_time * 2;
            break;
        case DHCP_REQUEST:  
            // start a timer for DHCPACK
            sink->timer = set_timer_sec(short_wait_time);
            break;
        case DHCP_BOUND:    
            // wait for timer, go to RENEWING when it fires
            sink->timer = set_timer_sec(dhcp_offer_lease_time >> 1);
            break;
        case DHCP_RENEW:    
            // periodically send REQUESTs; transitions as per REQUESTING
            break;
    }
}

static void dhcp_timer(packet_sink_t *s)
{
    printf("dhcp_timer(%d)\n", dhcp_state);

    switch(dhcp_state){
        case DHCP_DISCOVER: // shouldn't happen
            break;
        case DHCP_SELECT:
            // no DHCPOFFER received
            dhcp_enter_state(DHCP_DISCOVER);
            break;
        case DHCP_REQUEST:  
            // no DHCPACK received
            dhcp_enter_state(DHCP_DISCOVER);
            break;
        case DHCP_BOUND:    
            // 
            break;
        case DHCP_RENEW:    
            // periodically send REQUESTs; transitions as per REQUESTING
            break;
    }
}

static void dhcp_pump(packet_sink_t *s)
{
    packet_t *packet;

    printf("dhcp_pump(%d)\n", dhcp_state);

    while((packet = packet_queue_pophead(s->queue))){
        switch(dhcp_state){
        case DHCP_DISCOVER:
            break;
        case DHCP_SELECT:
            if(process_dhcp_reply(packet, dhcp_type_offer)){
                // skip any terrible offer
                if(!dhcp_offer_ipv4_address || !dhcp_offer_gateway || 
                        !dhcp_offer_dns_server || dhcp_offer_lease_time < 180)
                    break;

                /* TODO we clearly need to set more fields here, probably
                 * the BOOTP fields. might it be easier just to transform
                 * an OFFER into a REQUEST by swapping src/dst fields and
                 * changing the message type byte?
                 */

                // at this point we're happy with the offer, let's go!
                unsigned char req_opt[32];
                int req_opt_len = 0;

                req_opt[req_opt_len++] = dhcp_opt_server_id;
                req_opt[req_opt_len++] = 4;
                for(int i=0; i<4; i++)
                    req_opt[req_opt_len++] = dhcp_server_id[i];

                packet_t *req = packet_create_dhcp(ntohl(packet->ipv4->source_ip), dhcp_type_request,
                            req_opt, req_opt_len);

                // fiddle this
                req->ipv4->source_ip = htonl(dhcp_offer_ipv4_address);
                // TODO ARP should handle this!
                memcpy(req->eth->destination_mac, packet->eth->source_mac, sizeof(macaddr_t));
                req->flags |= packet_flag_destination_mac_valid;
                net_tx(req);

                select_wait_time = short_wait_time;
                dhcp_enter_state(DHCP_REQUEST);
            }
            break;
        case DHCP_REQUEST:  
            if(process_dhcp_reply(packet, dhcp_type_ack)){
                dhcp_enter_state(DHCP_BOUND);
                printf("DHCP lease: ip=0x%08lx, mask=0x%08lx, gw=0x%08lx, dns=0x%08lx, dur=%ld\n",
                        dhcp_offer_ipv4_address, dhcp_offer_subnet_mask, dhcp_offer_gateway,
                        dhcp_offer_dns_server, dhcp_offer_lease_time);
            }
            break;
        case DHCP_BOUND:    
            // wait for timer, go to RENEWING
            break;
        case DHCP_RENEW:    
            // periodically send REQUESTs; transitions as per REQUESTING
            break;
        }
        packet_free(packet);
    }
}

void dhcp_init(void)
{
    sink = packet_sink_alloc();
    sink->match_ethertype = ethertype_ipv4;
    sink->match_ipv4_protocol = ip_proto_udp;
    sink->match_local_port = 68;
    sink->cb_queue_pump = dhcp_pump;     // called when packets received
    sink->cb_timer_expired = dhcp_timer; // called on timer expiry
    net_add_packet_sink(sink);
    dhcp_enter_state(DHCP_DISCOVER);
    printf("dhcp_init done\n");
}
