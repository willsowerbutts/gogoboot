#include <types.h>
#include <stdlib.h>
#include "q40uart.h"
#include "timers.h"
#include "ff.h"
#include "tinyalloc.h"
#include "net.h"

typedef struct tftp_transfer_t tftp_transfer_t;

struct tftp_transfer_t {
    char *tftp_filename;
    char *disk_filename;
    FIL disk_file;
};

typedef struct tftp_header_t tftp_header_t;

struct __attribute__((packed, aligned(2))) tftp_header_t {
    uint16_t opcode;
    union payload {
        uint8_t raw[0];
        struct {
            uint16_t block_number;
            uint8_t data[];
        } data;
        struct {
            uint16_t block_number;
        } ack;
        struct {
            uint16_t error_code;
            uint8_t error_message[];
        } error;
    } payload;
};

static const uint16_t tftp_op_rrq = 1;
static const uint16_t tftp_op_wrq = 2;
static const uint16_t tftp_op_data = 3;
static const uint16_t tftp_op_ack = 4;
static const uint16_t tftp_op_err = 5;

packet_t *tftp_create_rrq(packet_sink_t *sink, const char *tftp_filename)
{
    int namelen = strlen(tftp_filename);
    int offset = 0;

    packet_t *packet = packet_create_for_sink(sink, namelen + 9); // <opcode>"filename"<0>"octet"<0>
    tftp_header_t *tftp = (tftp_header_t*)packet->data;

    tftp->opcode = htons(tftp_op_rrq);

    memcpy(tftp->payload.raw + offset, tftp_filename, namelen);
    offset += namelen;

    tftp->payload.raw[offset++] = 0;
    tftp->payload.raw[offset++] = 'o';
    tftp->payload.raw[offset++] = 'c';
    tftp->payload.raw[offset++] = 't';
    tftp->payload.raw[offset++] = 'e';
    tftp->payload.raw[offset++] = 't';
    tftp->payload.raw[offset++] = 0;

    return packet;
}

bool tftp_receive(uint32_t tftp_server_ip, const char *tftp_filename, const char *disk_filename)
{
    packet_sink_t *sink = packet_sink_alloc();
    tftp_transfer_t *tftp = malloc(sizeof(tftp_transfer_t));

    sink->match_interface_local_ip = true;
    sink->match_ipv4_protocol = ip_proto_udp;
    sink->match_remote_ip = tftp_server_ip;
    sink->match_remote_port = 69;
    sink->match_local_port = 8192 + (q40_read_timer_ticks() & 0x7fff);
    sink->sink_private = tftp;
    tftp->tftp_filename = strdup(tftp_filename);
    tftp->disk_filename = strdup(disk_filename);
    // TODO initialise tftp->disk_file with f_open 

    // set up callbacks
    // register the sink

    // while(tftp transfer incomplete){
    //     // monitor tftp state, print updates for user
    //     net_pump(); // but the actual work of the transfer is done in here via the data/timer callbacks
    // }

    // unregister the sink

    packet_sink_free(sink);
    free(tftp->tftp_filename);
    free(tftp->disk_filename);
    free(tftp);

    return true;
}
