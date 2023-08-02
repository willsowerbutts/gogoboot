#ifndef __NET_DOT_H__
#define __NET_DOT_H__

#include <q40types.h>
#include <stdbool.h>

typedef struct packet_t packet_t;
typedef struct packet_queue_t packet_queue_t;
typedef struct packet_sink_t packet_sink_t;
typedef struct ethernet_header_t ethernet_header_t;
typedef struct ipv4_header_t ipv4_header_t;
typedef struct udp_header_t udp_header_t;
typedef struct tcp_header_t tcp_header_t;
typedef struct icmp_header_t icmp_header_t;
typedef uint8_t macaddr_t[6];

extern macaddr_t const broadcast_macaddr;
static const uint32_t ipv4_broadcast = 0xffffffff;

extern macaddr_t interface_macaddr;
extern uint32_t interface_ipv4_address;
extern uint32_t interface_subnet_mask;
extern uint32_t interface_gateway;
extern uint32_t interface_dns_server;

extern uint32_t packet_alive_count;
extern uint32_t packet_discard_count;
extern uint32_t packet_bad_cksum_count;
extern uint32_t packet_rx_count;
extern uint32_t packet_tx_count;

struct packet_t {
    packet_t *next;               // used by packet_queue_t to create linked lists
    uint32_t flags;
    ethernet_header_t *eth;       // always set
    // arp_header_t *arp;         // set for arp
    ipv4_header_t *ipv4;          // set for ipv4 (all except arp)
    udp_header_t *udp;            // set for ipv4 udp
    tcp_header_t *tcp;            // set for ipv4 tcp
    icmp_header_t *icmp;          // set for ipv4 icmp
    uint16_t data_length;         // set for ipv4 udp, tcp
    uint8_t *data;                // set for ipv4 udp, tcp
    uint16_t buffer_length_alloc; // length allocated for buffer[]
    uint16_t buffer_length;       // length used by buffer[] (buffer_length <= length_alloc)
    uint8_t buffer[];             // must be final member of data structure
};

static const uint32_t packet_flag_destination_mac_valid = 1;

struct __attribute__((packed, aligned(2))) ethernet_header_t {
    macaddr_t destination_mac;
    macaddr_t source_mac;
    uint16_t ethertype;
    uint8_t payload[]; // 46 -- 1500 octets
};

static const uint16_t ethertype_ipv4 = 0x0800;
static const uint16_t ethertype_arp  = 0x0806;
static const uint16_t ethertype_vlan = 0x8100;
static const uint16_t ethertype_ipv6 = 0x86dd;

struct __attribute__((packed, aligned(2))) ipv4_header_t {
    uint8_t version_length;     // low 4 bits = 0100, top 4 bits = header length in 32-bit words
    uint8_t diffserv_ecn;       // ToS, ENC bits
    uint16_t length;            // length of total packet in bytes (before fragmentation)
    uint16_t id;                // identification
    uint16_t flags_and_frags;   // fragment offset and associated flags
    uint8_t ttl;                // time to live
    uint8_t protocol;           // identifies the next header type
    uint16_t checksum;          // one's complement, ugh, weak, how about a nice CRC16?
    uint32_t source_ip;         // our IPv4 address
    uint32_t destination_ip;    // their IPv4 address
    // uint32_t options[];      // optional options (we don't support them!)
    uint8_t payload[];          // next protocol header
};

static const uint8_t ip_proto_icmp = 1;
static const uint8_t ip_proto_tcp  = 6;
static const uint8_t ip_proto_udp  = 17;

struct __attribute__((packed, aligned(2))) udp_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;            // UDP header plus data
    uint16_t checksum;          // one's complement sum of pseudo-header (NOT real header) and data
    uint8_t payload[];          // finally we get to the actual user data
};

struct __attribute__((packed, aligned(2))) icmp_header_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t header_data[4];
    uint8_t payload[];          // finally we get to the actual user data
};

struct __attribute__((packed, aligned(2))) tcp_header_t {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t ack;
    uint8_t data_offset;
    uint16_t window_size;
    uint16_t checksum;
    // optional urgent pointer
    uint8_t options[];          // variable length
    // followed by the user data
};

#define PACKET_MAXLEN 1600      /* largest size we will process */
#define DEFAULT_TTL 64

struct packet_queue_t {
    packet_t *head;
    packet_t *tail;
};

struct packet_sink_t {
    packet_sink_t *next; // for linked lists
    void *sink_private;  // for sink's use

    uint32_t match_local_ip;
    uint32_t match_remote_ip;
    uint16_t match_local_port;
    uint16_t match_remote_port;
    uint16_t match_ethertype;
    uint8_t  match_ipv4_protocol;

    packet_queue_t *queue;
    void (*cb_queue_pump)(packet_sink_t *sink);

    timer_t timer;
    void (*cb_timer_expired)(packet_sink_t *sink);
};

/* ne2000.c */
bool eth_init(void); // returns true if card found
void eth_halt(void);
void eth_pump(void); // called from net_pump

/* net.c -- interface with ne2000.c */
void net_eth_push(packet_t *packet);
packet_t *net_eth_pull(void);
void net_add_packet_sink(packet_sink_t *c);
void net_remove_packet_sink(packet_sink_t *c);

/* net.c */
void net_init(void);
void net_pump(void);
void net_tx(packet_t *packet);
void net_dump_packet_sinks(void);

/* packet.c */
packet_t *packet_alloc(int buffer_size);
packet_t *packet_create_tcp(uint32_t dest_ipv4, uint16_t destination_port, uint16_t source_port, int data_size);
packet_t *packet_create_udp(uint32_t dest_ipv4, uint16_t destination_port, uint16_t source_port, int data_size);
packet_t *packet_create_icmp(uint32_t dest_ipv4, int data_size);
void packet_set_destination_mac(packet_t *packet, const macaddr_t *mac);
void packet_free(packet_t *packet);

packet_queue_t *packet_queue_alloc(void);
void packet_queue_free(packet_queue_t *q);
int packet_queue_length(packet_queue_t *q);
void packet_queue_addtail(packet_queue_t *q, packet_t *p);
packet_t *packet_queue_peekhead(packet_queue_t *q);
packet_t *packet_queue_pophead(packet_queue_t *q);

packet_sink_t *packet_sink_alloc(void);
void packet_sink_free(packet_sink_t *s);

void net_compute_ipv4_checksum(packet_t *packet);
void net_compute_icmp_checksum(packet_t *packet);
void net_compute_udp_checksum(packet_t *packet);
void net_compute_tcp_checksum(packet_t *packet);

bool net_verify_ipv4_checksum(packet_t *packet);
bool net_verify_icmp_checksum(packet_t *packet);
bool net_verify_udp_checksum(packet_t *packet);
bool net_verify_tcp_checksum(packet_t *packet);

/* dhcp.c */
void dhcp_init(void);

/* icmp.c */
void net_icmp_init(void);
void net_icmp_send_unreachable(packet_t *packet);

#endif
