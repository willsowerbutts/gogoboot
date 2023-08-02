#ifndef __DHCP_INTERNALS_DOT_H__
#define __DHCP_INTERNALS_DOT_H__

#include <q40types.h>

static const int short_wait_time = 5; // seconds

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

const uint8_t magic_cookie_value[4] = {0x63, 0x82, 0x53, 0x63};

static const uint8_t dhcp_opt_pad           = 0x00;
static const uint8_t dhcp_opt_subnet_mask   = 0x01;
static const uint8_t dhcp_opt_gateway       = 0x03;
static const uint8_t dhcp_opt_dns_server    = 0x06;
static const uint8_t dhcp_opt_domain_name   = 0x15;
static const uint8_t dhcp_opt_lease_time    = 0x33;
static const uint8_t dhcp_opt_message_type  = 0x35;
static const uint8_t dhcp_opt_server_id     = 0x36;
static const uint8_t dhcp_opt_param_request = 0x37;
static const uint8_t dhcp_opt_max_size      = 0x39;
static const uint8_t dhcp_opt_terminator    = 0xff;

static const uint8_t dhcp_type_discover = 1;
static const uint8_t dhcp_type_offer    = 2;
static const uint8_t dhcp_type_request  = 3;
static const uint8_t dhcp_type_decline  = 4;
static const uint8_t dhcp_type_ack      = 5;
static const uint8_t dhcp_type_nack     = 6;
static const uint8_t dhcp_type_release  = 7;

#endif
