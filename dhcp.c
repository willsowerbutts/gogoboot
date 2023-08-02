/* (c) 2023 William R Sowerbutts <will@sowerbutts.com> */

#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "cli.h"
#include "net.h"
#include "dhcp_internals.h"

#undef DHCP_DEBUG

const int renew_retry_count = 100;
int select_wait_time = short_wait_time;

static uint32_t dhcp_server_id;
static uint32_t dhcp_offer_ipv4_address;
static uint32_t dhcp_offer_subnet_mask;
static uint32_t dhcp_offer_gateway;
static uint32_t dhcp_offer_dns_server;
static uint32_t dhcp_offer_lease_time;
int renew_retry_remaining;

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

#ifdef DHCP_DEBUG
    printf("packet_create_dhcp: message_type=%d\n", message_type);
#endif

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
    memcpy(d->chaddr, interface_macaddr, 6);
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

static void dhcp_send_request(void)
{
    unsigned char req_opt[32];
    int req_opt_len = 0;

    req_opt[req_opt_len++] = dhcp_opt_requested_ip;
    req_opt[req_opt_len++] = 4;
    *((uint32_t*)(req_opt+req_opt_len)) = htonl(dhcp_offer_ipv4_address);
    req_opt_len += 4;

    req_opt[req_opt_len++] = dhcp_opt_server_id;
    req_opt[req_opt_len++] = 4;
    *((uint32_t*)(req_opt+req_opt_len)) = htonl(dhcp_server_id);
    req_opt_len += 4;

    // the DHCPREQUEST should also go to the broadcast address
    packet_t *req = packet_create_dhcp(ipv4_broadcast, dhcp_type_request,
            req_opt, req_opt_len);

    net_tx(req);
}

bool process_dhcp_reply(packet_t *packet, uint8_t expected_dhcp_type)
{
    int offset, length;
    uint8_t opt_code, opt_len, *opt_data;
    dhcp_message_t *d = (dhcp_message_t*)packet->data;

    if(d->op != 2) // check for BOOTREPLY
        return false;

    // parse DHCP options
    offset = 4; // skip over magic cookie
    length = packet->data_length - sizeof(dhcp_message_t);

    // check for DHCP magic cookie
    if(length <= 4 ||
            d->options[0] != 0x63 ||
            d->options[1] != 0x82 ||
            d->options[2] != 0x53 ||
            d->options[3] != 0x63)
        return false;

    // reset offer state
    dhcp_server_id = 0;
    dhcp_offer_ipv4_address = 0;
    dhcp_offer_subnet_mask = 0;
    dhcp_offer_gateway = 0;
    dhcp_offer_dns_server = 0;
    dhcp_offer_ipv4_address = ntohl(d->yiaddr);

    // process DHCP options
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
                    dhcp_server_id = ntohl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_subnet_mask:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_subnet_mask = ntohl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_gateway:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_gateway = ntohl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_lease_time:
                    if(opt_len != 4)
                        return false;
                    dhcp_offer_lease_time = ntohl(*((uint32_t*)opt_data));
                    break;
                case dhcp_opt_dns_server:
                    if(opt_len < 4)
                        return false;
                    dhcp_offer_dns_server = ntohl(*((uint32_t*)opt_data));
                    break;
                default:
#ifdef DHCP_DEBUG
                    printf("dhcp: ignored option %02x len %02x:", opt_code, opt_len);
                    for(int i=0; i<opt_len; i++)
                        printf(" %02x", opt_data[i]);
                    printf("\n");
#endif
                    break;
            }
        }
        offset += 2 + opt_len;
    }

#ifdef DHCP_DEBUG
    printf("process_dhcp_reply: message_type=%d OK!\n", expected_dhcp_type);
#endif

    return true;
}

static void dhcp_enter_state(dhcp_state_t state)
{
    dhcp_state = state;

#ifdef DHCP_DEBUG
    printf("dhcp_enter_state %d\n", state);
#endif

    switch(dhcp_state){
        case DHCP_DISCOVER:
            // create and send a DHCPDISCOVER message
            net_tx(packet_create_dhcp(ipv4_broadcast, dhcp_type_discover,
                        discover_options, sizeof(discover_options)));
            dhcp_enter_state(DHCP_SELECT);
            break;
        case DHCP_SELECT:
            // start a timer for DHCPOFFER
            sink->timer = set_timer_sec(select_wait_time);
            // increase the time we wait in future cycles, up to ~2 minutes.
            // this is reset when we send a DHCPREQUEST (in response to a DHCPOFFER)
            if(select_wait_time < 120)
                select_wait_time = select_wait_time * 2;
            break;
        case DHCP_REQUEST:
            // start a timer for DHCPACK
            sink->timer = set_timer_sec(short_wait_time);
            break;
        case DHCP_BOUND:
            // relax and wait for our lease to near expiry
            sink->timer = set_timer_sec(dhcp_offer_lease_time - (renew_retry_count * short_wait_time));
            break;
        case DHCP_RENEW:
            // periodically send REQUESTs
            renew_retry_remaining = renew_retry_count;
            sink->timer = set_timer_sec(short_wait_time);
            dhcp_send_request();
            break;
    }
}

static void dhcp_timer(packet_sink_t *sink)
{
#ifdef DHCP_DEBUG
    printf("dhcp_timer %d\n", dhcp_state);
#endif

    switch(dhcp_state){
        case DHCP_DISCOVER:
            // this should not happen; we don't wait around in this state
            break;
        case DHCP_SELECT:
            // no DHCPOFFER received? retransit the DHCPDISCOVER.
            dhcp_enter_state(DHCP_DISCOVER);
            break;
        case DHCP_REQUEST:
            // no DHCPACK received? restart the process.
            dhcp_enter_state(DHCP_DISCOVER);
            break;
        case DHCP_BOUND:
            // Our lease is nearing expiry; we need to renew it.
            dhcp_enter_state(DHCP_RENEW);
            break;
        case DHCP_RENEW:
            // periodically send REQUESTs
            renew_retry_remaining--;
            if(renew_retry_remaining > 0){
                dhcp_send_request();
                // reset the timer so we get called again
                sink->timer = set_timer_sec(short_wait_time);
            }else
                dhcp_enter_state(DHCP_DISCOVER); // lease expired -- start over
            break;
    }
}

static void dhcp_pump(packet_sink_t *s)
{
    packet_t *packet;

    while((packet = packet_queue_pophead(s->queue))){
        switch(dhcp_state){
        case DHCP_DISCOVER:
        case DHCP_BOUND:
            break;
        case DHCP_SELECT:
            if(process_dhcp_reply(packet, dhcp_type_offer)){
                // skip unsuitable offers
                if(!dhcp_offer_ipv4_address || !dhcp_offer_gateway ||
                        !dhcp_offer_dns_server || dhcp_offer_lease_time < 180)
                    break;
                // at this point we're happy with the offer, let's go!
                dhcp_send_request();
                select_wait_time = short_wait_time;
                dhcp_enter_state(DHCP_REQUEST);
            }
            break;
        case DHCP_REQUEST:
        case DHCP_RENEW:
            if(process_dhcp_reply(packet, dhcp_type_nack)){
                dhcp_enter_state(DHCP_DISCOVER); // start over again on any NACK
            }else if(process_dhcp_reply(packet, dhcp_type_ack)){
                interface_ipv4_address = dhcp_offer_ipv4_address;
                interface_subnet_mask = dhcp_offer_subnet_mask;
                interface_gateway = dhcp_offer_gateway;
                interface_dns_server = dhcp_offer_dns_server;
                if(dhcp_state == DHCP_REQUEST){ // don't print this on every RENEW
                    printf("DHCP lease acquired (%d.%d.%d.%d, %dh %dm)\n",
                            (int)(interface_ipv4_address >> 24 & 0xff),
                            (int)(interface_ipv4_address >> 16 & 0xff),
                            (int)(interface_ipv4_address >>  8 & 0xff),
                            (int)(interface_ipv4_address       & 0xff),
                            (int)(dhcp_offer_lease_time / 3600),
                            (int)(dhcp_offer_lease_time % 3600)/60);
                }
                dhcp_enter_state(DHCP_BOUND);
            }
            break;
        }
        packet_free(packet);
    }
}

void dhcp_init(void)
{
    sink = packet_sink_alloc();
    sink->match_ethertype = htons(ethertype_ipv4);
    sink->match_ipv4_protocol = htons(ip_proto_udp);
    sink->match_local_port = htons(68);
    sink->cb_queue_pump = dhcp_pump;     // called when packets received
    sink->cb_timer_expired = dhcp_timer; // called on timer expiry
    net_add_packet_sink(sink);
    dhcp_enter_state(DHCP_DISCOVER);
}
