#ifndef __NET_DOT_H__
#define __NET_DOT_H__

#include <types.h>
#include <timers.h>

typedef struct packet_t packet_t;
typedef struct packet_queue_t packet_queue_t;
typedef struct packet_sink_t packet_sink_t;
typedef struct ethernet_header_t ethernet_header_t;
typedef struct arp_header_t arp_header_t;
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
extern uint32_t interface_ipv4_gateway;
extern uint32_t interface_dns_server;

extern uint32_t packet_alive_count;
extern uint32_t packet_discard_count;
extern uint32_t packet_bad_cksum_count;
extern uint32_t packet_rx_count;
extern uint32_t packet_tx_count;

struct packet_t {
    packet_t *next;               // used by packet_queue_t to create linked lists
    uint32_t flags;               // flag bits (packet_flag_*)
    uint32_t ipv4_nexthop;        // address we're going to ARP for
    ethernet_header_t *eth;       // always set
    arp_header_t *arp;            // set for arp
    ipv4_header_t *ipv4;          // set for ipv4
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
static const uint32_t packet_flag_nexthop_resolved = 2;

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

struct __attribute__((packed, aligned(2))) arp_header_t {
    uint16_t  hardware_type;
    uint16_t  protocol_type;
    uint8_t   hardware_length;
    uint8_t   protocol_length;
    uint16_t  operation;
    macaddr_t sender_mac;
    uint32_t  sender_ip;
    macaddr_t target_mac;
    uint32_t  target_ip;
};

static const uint16_t arp_op_request = 1;
static const uint16_t arp_op_reply = 2;

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

#define PACKET_MAXLEN 1536      /* largest size we will process */
#define DEFAULT_TTL 64

struct packet_queue_t {
    packet_t *head;
    packet_t *tail;
};

struct packet_sink_t {
    packet_sink_t *next; // for linked lists
    void *sink_private;  // for sink's use

    bool match_interface_local_ip; // similar to match_local_ip, but using the current 'interface_ipv4_address'
    uint32_t match_local_ip;       // ... whereas this matches one specific IP
    uint32_t match_remote_ip;
    uint16_t match_local_port;
    uint16_t match_remote_port;
    uint16_t match_ethertype;
    uint8_t  match_ipv4_protocol;

    uint32_t packets_queued;
    packet_queue_t queue;
    void (*cb_packet_received)(packet_sink_t *sink, packet_t *packet);

    timer_t timer;
    void (*cb_timer_expired)(packet_sink_t *sink);
};

/* ne2000.c */
bool eth_init(void); // returns true if card found
void eth_halt(void);
void eth_pump(void); // called from net_pump
bool eth_attempt_tx(packet_t *packet); // returns true if transmission started; caller must free packet.

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
packet_t *packet_create_for_sink(packet_sink_t *sink, int data_size);
void packet_set_destination_mac(packet_t *packet, const macaddr_t *mac);
void packet_free(packet_t *packet);
uint32_t net_parse_ipv4(const char *str);

// for dynamically allocated queues
packet_queue_t *packet_queue_alloc(void); // calls packet_queue_init
void packet_queue_free(packet_queue_t *q); // calls packet_queue_drain
void packet_queue_init(packet_queue_t *q);
void packet_queue_drain(packet_queue_t *q);

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

/* arp.c */
typedef enum { arp_okay, arp_wait, arp_fail } arp_result_t;
void net_arp_init(void);
arp_result_t net_arp_resolve(packet_t *packet);

/* tftp.c */
bool tftp_receive(uint32_t tftp_server_ip, const char *tftp_filename, const char *disk_filename);

#endif
