#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "cli.h"
#include "net.h"

typedef enum { // start in DHCP_INIT
    DHCP_DISCOVER, // create and send a DISCOVER message, start timer, go to SELECTING
    DHCP_SELECT,   // wait for timer, pick best offer, send REQUEST, go to REQUESTING, if no offers go to INIT
    DHCP_REQUEST,  // receive ACK, start timer, go to BOUND; on NACK or timeout go to init
    DHCP_BOUND,    // wait for timer, go to RENEWING
    DHCP_RENEW,    // periodically send REQUESTs; transitions as per REQUESTING
} dhcp_state_t;

typedef struct __attribute__((packed, aligned(2))) {
    uint8_t op;         // opcode/message type (1=BOOTREQUEST, 2=BOOTREPLY)
    uint8_t htype;      // hardware address type (1 for ethernet)
    uint8_t hlen;       // hardware address length (6 for ethernet)
    uint8_t hops;       // clients set this to zero
    uint32_t xid;       // transaction id (random number)
    uint16_t secs;      // seconds since client began address resolution or renewal
    uint16_t flags;     // pretty much what it says on the tin
    uint32_t ciaddr;    // client IP address (if we are bound to one already)
    uint32_t yiaddr;    // "your" (client) IP address
    uint32_t siaddr;    // IP of next server to use in bootstrap process
    uint32_t giaddr;    // relay agent IP address
    uint8_t chaddr[16]; // client hardware address
    uint8_t sname[64];  // optional server hostname
    uint8_t file[128];  // boot file name
    uint8_t options[];  // options (variable length, client must support receiving at least 312 bytes)
} dhcp_message_t;

static dhcp_state_t dhcp_state;
static timer_t dhcp_timer;
static int retransmits;

/* see https://www.rfc-editor.org/rfc/rfc1533 in particular section 9 */
uint8_t const dhcp_dhcpdiscover_options[] = {
    // magic cookie
    0x63, 0x82, 0x53, 0x63,
    // DHCP message type - DHCPDISCOVER 
    0x35, 0x01, 0x01,
    // DHCP parameter request list (1=subnet mask, 3=router, 6=nameserver, 15=domain name)
    0x37, 0x04, 0x01, 0x03, 0x06, 0x0f,
    // maximum DHCP message size - 1400 bytes
    0x39, 2, 0x05, 0x78,
    // terminator
    0xff                          
};

packet_t *create_dhcpdiscover(void)
{
    packet_t *p = packet_alloc(sizeof(ethernet_header_t) + 
                               sizeof(ipv4_header_t) +
                               sizeof(udp_header_t) +
                               sizeof(dhcp_message_t) +
                               sizeof(dhcp_dhcpdiscover_options));

    ethernet_header_t *e = (ethernet_header_t*)p->data;
    ipv4_header_t *i = (ipv4_header_t*)e->payload;
    udp_header_t *u = (udp_header_t*)i->payload;
    dhcp_message_t *d = (dhcp_message_t*)u->payload;

    // set up ethernet header
    memset(e->destination_mac, 0xff, 6);
    memcpy(e->source_mac, eth_get_interface_mac(), 6);
    e->ethertype = htons(ethertype_ipv4);

    // set up ipv4 header
    i->version_length = 0x45;   // we don't use options so length = 5 x 4 = 20 bytes
    i->diffserv_ecn = 0;
    i->length = htons(sizeof(ipv4_header_t) + 
                      sizeof(udp_header_t) +
                      sizeof(dhcp_message_t) +
                      sizeof(dhcp_dhcpdiscover_options));
    i->id = htons(q40_read_timer_ticks() & 0xffff);
    i->flags_and_frags = htons(0x4000); // don't fragment
    i->ttl = DEFAULT_TTL;
    i->protocol = ip_proto_udp;
    i->source_ip = htonl(0);
    i->destination_ip = htonl(0xffffffff);
    net_compute_ipv4_checksum(i);

    // set up udp header
    u->source_port = htons(68);
    u->destination_port = htons(67);
    u->length = htons(sizeof(udp_header_t) + 
                      sizeof(dhcp_message_t) +
                      sizeof(dhcp_dhcpdiscover_options));
    u->checksum = 0; // udp checksum is optional, skip it for now

    d->op = 1;          // BOOTREQUEST
    d->htype = 1;       // 10M ethernet
    d->hlen = 6;        // ethernet
    d->hops = 0;
    d->xid = q40_read_timer_ticks(); // no need to byte swap since it's random anyway
    d->secs = ntohs(0);        // we can do better here, surely?
    d->flags = ntohs(0);
    d->ciaddr = d->yiaddr = d->siaddr = d->giaddr = htonl(0);
    memcpy(d->chaddr, eth_get_interface_mac(), 6);
    memset(d->chaddr+6, 0, 10 + 64 + 128); // zero residue of chaddr, plus sname and file fields together
    memcpy(d->options, dhcp_dhcpdiscover_options, sizeof(dhcp_dhcpdiscover_options));

    return p;
}

static void dhcp_enter_state(dhcp_state_t state)
{
    dhcp_state = state;
    retransmits = 0;

    switch(state){
        case DHCP_DISCOVER:
            net_tx(create_dhcpdiscover());
            dhcp_enter_state(DHCP_SELECT);
        default:break;
    }

}

void dhcp_pump(void)
{
    switch(dhcp_state)
    {
        default:break;

        case DHCP_BOUND:
                if(timer_expired(dhcp_timer))
                    printf("oh noes");
    }
}

void dhcp_init(void)
{
    dhcp_enter_state(DHCP_DISCOVER);
}
