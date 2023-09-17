#include <types.h>
#include <stdlib.h>
#include <uart.h>
#include <timers.h>
#include <fatfs/ff.h>
#include <cli.h>
#include <net.h>

// documentation:
// https://www.rfc-editor.org/rfc/rfc1350 - TFTP Protocol (Revision 2)
// https://www.rfc-editor.org/rfc/rfc2347 - TFTP Option Extension
// https://www.rfc-editor.org/rfc/rfc2349 - TFTP Timeout Interval and Transfer Size Options
// https://www.rfc-editor.org/rfc/rfc7440 - TFTP Windowsize Option
// https://www.compuphase.com/tftp.htm - Extending TFTP

#define REQUEST_TIMEOUT 1500 // ms
#define DATA_TIMEOUT     250 // ms

typedef struct tftp_transfer_t tftp_transfer_t;

struct tftp_transfer_t {
    packet_queue_t data_queue;
    FIL disk_file;
    bool is_put;
    char *tftp_filename;
    char *disk_filename;
    uint16_t block_size;
    uint16_t last_block;
    uint16_t last_ack;
    uint16_t rollover_value;
    int bytes_transferred;
    int total_size;
    int window_size;
    bool started;
    bool completed;
    bool success;
    int timeouts;
    int retransmits_this_block;
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
static const uint16_t tftp_op_options_ack = 6;

static uint16_t expected_block_number(tftp_transfer_t *tftp, int count)
{
    int expected_block;

    expected_block = (int)tftp->last_block + count;
    if(expected_block > 0xffff){
        expected_block -= 0x10000;
        /* deal with rollover_value == 1 */
        if(expected_block <= count && tftp->rollover_value == 1)
            expected_block++;
    }
    
    return expected_block;
}

#define MAXOPT 1400
static int options_append(char *options, int offset, char *extra)
{
    int extra_len = strlen(extra) + 1;

    if(extra_len+offset >= MAXOPT){
        printf("tftp: options too long!");
        return 0;
    }

    memcpy(options+offset, extra, extra_len);
    return offset + extra_len;
}

static packet_t *tftp_create_request(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;
    char options[MAXOPT];
    int offset = 0;
    int windowsize;
    char wsize[2];

    /* when receiving, try to avoid overflowing ethernet device receive buffer */
    /* no issue on transmit path */
    /* 5 * 256 = 1280 bytes; allows 1024 byte payload + up to 210 headers etc */
    if(tftp->is_put)
        windowsize = 8;
    else
        windowsize = eth_rxbuffer_size() / (256 * 5); 

    if(windowsize > 8) /* 8 works really well */
        windowsize = 8;
    if(windowsize < 1) /* need at least 1 */
        windowsize = 1;

    wsize[0] = '0' + windowsize;
    wsize[1] = 0;

    offset = options_append(options, offset, tftp->tftp_filename);
    offset = options_append(options, offset, "octet");

    offset = options_append(options, offset, "rollover");
    offset = options_append(options, offset, "0");

    offset = options_append(options, offset, "tsize");
    if(tftp->is_put){
        uint32_t n = tftp->total_size;
        char sizebuf[12];
        char *t = sizebuf+sizeof(sizebuf);  
        *--t = 0; 
        do { 
            *--t = (n % 10)+'0'; 
            n/=10; 
        } while(n);
        offset = options_append(options, offset, t);
    }else{
        /* get request: 0 = please tell me total file size */
        offset = options_append(options, offset, "0");
    }

    offset = options_append(options, offset, "blksize");
    offset = options_append(options, offset, "1024");

    offset = options_append(options, offset, "windowsize");
    offset = options_append(options, offset, wsize);

    packet_t *packet = packet_create_for_sink(sink, offset + 2);
    packet->udp->destination_port = htons(69); // RRQ/WRQ always goes to server port 69
    tftp_header_t *message = (tftp_header_t*)packet->data;
    message->opcode = htons(tftp->is_put ? tftp_op_wrq : tftp_op_rrq);
    memcpy(message->payload.raw, options, offset);

    return packet;
}

static packet_t *tftp_create_ack(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;

    packet_t *packet = packet_create_for_sink(sink, 4);
    tftp_header_t *message = (tftp_header_t*)packet->data;

    message->opcode = htons(tftp_op_ack);
    message->payload.ack.block_number = htons(tftp->last_block);
    tftp->last_ack = tftp->last_block;

    return packet;
}

static packet_t *tftp_create_data(packet_sink_t *sink, uint16_t block_number)
{
    tftp_transfer_t *tftp = sink->sink_private;

    packet_t *packet = packet_create_for_sink(sink, tftp->block_size + 4);
    tftp_header_t *message = (tftp_header_t*)packet->data;

    message->opcode = htons(tftp_op_data);
    message->payload.data.block_number = htons(block_number);

    return packet;
}

static void tftp_put_send_data(packet_sink_t *sink, int count)
{
    tftp_transfer_t *tftp = sink->sink_private;
    bool last_block = false;
    packet_t *packet;
    tftp_header_t *message;
    FRESULT fr;
    UINT size;

    f_lseek(&tftp->disk_file, tftp->bytes_transferred);

    for(int n=0; !last_block && n < count; n++){
        packet = tftp_create_data(sink, expected_block_number(tftp, n + 1));
        message = (tftp_header_t*)packet->data;
        fr = f_read(&tftp->disk_file, message->payload.data.data, tftp->block_size, &size);
        if(fr != FR_OK){
            printf("tftp: failed to read from \"%s\": %s\n", tftp->disk_filename, f_errmsg(fr));
            tftp->completed = true;
            tftp->success = false;
            packet_free(packet);
            return;
        }
        if(size < tftp->block_size){
            if(!packet_data_resize(packet, size + 4)){
                printf("tftp: resize failed\n");
                packet_free(packet);
                return;
            }
            last_block = true;
        }
        net_tx(packet);
    }

    sink->timer = set_timer_ms(DATA_TIMEOUT);
}

static void tftp_put_process_ack(packet_sink_t *sink, packet_t *packet)
{
    tftp_transfer_t *tftp = sink->sink_private;
    tftp_header_t *message = (tftp_header_t*)packet->data;
    int blocks_transferred;
    uint16_t block;

    block = ntohs(message->payload.ack.block_number);

    for(blocks_transferred = tftp->window_size; blocks_transferred >= 0; blocks_transferred--){
        if(block == expected_block_number(tftp, blocks_transferred))
            break;
    }

    if(blocks_transferred < 0){
        printf("tftp: unexpected ack %d (last %d)\n", block, tftp->last_block);
        return;
    }

    if(blocks_transferred > 0)
        tftp->retransmits_this_block = 0; 

    tftp->last_block = block;
    tftp->bytes_transferred += ((int)tftp->block_size * (int)blocks_transferred);

    if(tftp->bytes_transferred > tftp->total_size){ /* note > is correct here */
        // we're done!
        tftp->completed = true;
        tftp->success = true;
    }else{
        // send more
        tftp_put_send_data(sink, (blocks_transferred == tftp->window_size) ? tftp->window_size : 1);
    }
}

static void tftp_get_send_ack(packet_sink_t *sink)
{
    net_tx(tftp_create_ack(sink));
    sink->timer = set_timer_ms(DATA_TIMEOUT);
}

static void tftp_process_options_ack(packet_sink_t *sink, tftp_header_t *message, int message_len)
{
    tftp_transfer_t *tftp = sink->sink_private;
    char *opt, *val, *ptr, *end;
    int val_int;

    // message->opcode has been confirmed to be tftp_op_options_ack already

    ptr = (char*)message->payload.raw;
    end = ptr+message_len-2;

    printf("tftp: options ack:");

    while(true){
        // get option
        opt = ptr;
        while(*ptr && ptr < end)
            ptr++;
        if(ptr >= end)
            break;
        ptr++;
        // get value
        val = ptr;
        while(*ptr && ptr < end)
            ptr++;
        if(ptr >= end)
            break;
        ptr++;
        // process option + value
        val_int = atoi(val);
        if(!strcmp(opt, "rollover") && (val_int == 0 || val_int == 1)){
            tftp->rollover_value = val_int;
        }else if(!strcmp(opt, "tsize")){
            if(!tftp->is_put)
                tftp->total_size = val_int;
        }else if(!strcmp(opt, "blksize")){
            tftp->block_size = val_int;
        }else if(!strcmp(opt, "windowsize")){
            tftp->window_size = val_int;
        }
        printf(" %s=%d", opt, val_int);
    }

    putchar('\n');

    if(tftp->is_put){
        // for sending files, send our first DATA packets to agree to the options
        tftp_put_send_data(sink, tftp->window_size);
    }else{
        // for receiving files, send an ACK with block=0 to agree to the options
        tftp_get_send_ack(sink);
    }
}

static void tftp_get_flush_data_and_ack(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;
    packet_t *packet;
    tftp_header_t *message;
    int size;
    FRESULT fr;

    // send this FIRST so we can overlap receiving more data with writing to disk
    tftp_get_send_ack(sink);

    // then flush any buffered packets to file on disk
    while((packet = packet_queue_pophead(&tftp->data_queue))){
        message = (tftp_header_t*)packet->data;
        size = packet->data_length - 4;

        if(size > 0){
            fr = f_write(&tftp->disk_file, message->payload.data.data, size, NULL);
            tftp->bytes_transferred += size;
            if(fr != FR_OK){
                printf("tftp: failed to write to \"%s\": %s\n", tftp->disk_filename, f_errmsg(fr));
                tftp->completed = true;
                tftp->success = false;
            }
        }
        packet_free(packet);
    }
}

static bool tftp_get_process_data(packet_sink_t *sink, packet_t *packet)
{
    tftp_transfer_t *tftp = sink->sink_private;
    tftp_header_t *message = (tftp_header_t*)packet->data;
    int size;
    uint16_t rxblock;
    bool free_packet = true;

    size = packet->data_length - 4;
    rxblock = ntohs(message->payload.data.block_number);

    if(rxblock == expected_block_number(tftp, 1)){ // is it the block we are expecting?
        tftp->last_block = rxblock;
        tftp->retransmits_this_block = 0;

        packet_queue_addtail(&tftp->data_queue, packet);
        free_packet = false;

        if(!tftp->completed && size < tftp->block_size){
            // a short data block indicates success
            tftp->completed = true;
            tftp->success = true;
            tftp_get_flush_data_and_ack(sink);
        }
    }

    if(tftp->last_block == ((tftp->last_ack + tftp->window_size) & 0xffff))
        tftp_get_flush_data_and_ack(sink);

    return free_packet;
}

static void tftp_client_packet_received(packet_sink_t *sink, packet_t *packet)
{
    bool free_packet = true;
    tftp_transfer_t *tftp = sink->sink_private;
    tftp_header_t *message = (tftp_header_t*)packet->data;

    if(!tftp->started){
        // lock on to the server's source port
        net_remove_packet_sink(sink);
        sink->match_remote_port = ntohs(packet->udp->source_port);
        net_add_packet_sink(sink);
        printf("tftp: server using port %d\n", sink->match_remote_port);
        tftp->started = true;
    }

    switch(ntohs(message->opcode)){
        case tftp_op_options_ack:
            tftp_process_options_ack(sink, message, packet->data_length);
            break;
        case tftp_op_ack:
            if(tftp->is_put)
                tftp_put_process_ack(sink, packet);
            else
                printf("tftp: unexpected ACK packet during get?\n");
            break;
        case tftp_op_data:
            if(tftp->is_put)
                printf("tftp: unexpected DATA packet during put?\n");
            else
                free_packet = tftp_get_process_data(sink, packet);
            break;
        case tftp_op_err:
            printf("tftp: server error code 0x%0x: %s\n",
                    ntohs(message->payload.error.error_code), message->payload.error.error_message);
            tftp->completed = true;
            tftp->success = false;
            break;
        default:
            printf("tftp: unexpected server response, opcode 0x%04x\n", ntohs(message->opcode));
            pretty_dump_memory(packet->buffer, packet->buffer_length);
            tftp->completed = true;
            tftp->success = false;
            break;
    }

    if(free_packet)
        packet_free(packet);
}

static void tftp_client_timer_expired(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;

    if(tftp->retransmits_this_block > 20){
        printf("tftp: too many retransmissions\n");
        tftp->completed = true;
        tftp->success = false;
        return;
    }

    if(tftp->completed)
        return;

    if(!tftp->started){
        sink->timer = set_timer_ms(REQUEST_TIMEOUT);
        net_tx(tftp_create_request(sink));
    }else{
        if(tftp->is_put)
            tftp_put_send_data(sink, 1);
        else
            tftp_get_flush_data_and_ack(sink);
    }

    tftp->timeouts++;
    tftp->retransmits_this_block++;
}

bool tftp_transfer(uint32_t tftp_server_ip, const char *tftp_filename, 
        const char *disk_filename, bool is_put)
{
    FRESULT fr;
    uint32_t start, taken, rate;
    int uart_byte, reported_transferred;
    packet_sink_t *sink = packet_sink_alloc();
    tftp_transfer_t *tftp = malloc(sizeof(tftp_transfer_t));
    memset(tftp, 0, sizeof(tftp_transfer_t));
    packet_queue_init(&tftp->data_queue);

    sink->match_interface_local_ip = true;
    sink->match_ipv4_protocol = ip_proto_udp;
    sink->match_remote_ip = tftp_server_ip;
    sink->match_local_port = 8192 + (gogoboot_read_timer() & 0x7fff);
    sink->sink_private = tftp;
    tftp->last_block = 0;
    tftp->block_size = 512;
    tftp->window_size = 1;
    tftp->is_put = is_put;
    tftp->tftp_filename = strdup(tftp_filename);
    tftp->disk_filename = strdup(disk_filename);

    if(is_put){
        fr = f_open(&tftp->disk_file, tftp->disk_filename, FA_READ);
        tftp->total_size = f_size(&tftp->disk_file);
    }else
        fr = f_open(&tftp->disk_file, tftp->disk_filename, FA_WRITE | FA_CREATE_ALWAYS);

    if(fr != FR_OK){
        printf("tftp: failed to open \"%s\": %s\n", tftp->disk_filename, f_errmsg(fr));
    }else{
        printf("tftp: %s %d.%d.%d.%d:%s %s local file \"%s\"",
                is_put ? "put" : "get",
                (int)(tftp_server_ip >> 24 & 0xff),
                (int)(tftp_server_ip >> 16 & 0xff),
                (int)(tftp_server_ip >>  8 & 0xff),
                (int)(tftp_server_ip       & 0xff),
                tftp->tftp_filename,
                is_put ? "from" : "to",
                tftp->disk_filename);
        if(is_put)
            printf(" %d bytes", tftp->total_size);
        putchar('\n');

        start = gogoboot_read_timer();
        sink->cb_packet_received = tftp_client_packet_received;
        sink->cb_timer_expired = tftp_client_timer_expired;
        net_add_packet_sink(sink);
        tftp_client_timer_expired(sink); // synthesise a timeout; triggers transmission of RRQ/WRQ
        tftp->timeouts = 0; // fixup counts, since our "timeout" was synthetic
        tftp->retransmits_this_block = 0; 

        printf("Transfer started: Press Q to abort\n");

        reported_transferred = 0;
        while(!tftp->completed){
            net_pump(); // this calls our callsbacks to make the transfer go
            uart_byte = uart_read_byte();
            if(uart_byte == 'q' || uart_byte == 'Q'){
                printf("Aborted.\n");
                break;
            }
            if((tftp->bytes_transferred - reported_transferred) >= (256*1024) || 
               (tftp->total_size && tftp->bytes_transferred >= tftp->total_size)){
                reported_transferred = tftp->bytes_transferred;
                if(tftp->total_size){
                    if(reported_transferred > tftp->total_size)
                        reported_transferred = tftp->total_size;
                    printf("tftp: %d/%d KB", reported_transferred >> 10, tftp->total_size >> 10);
                }else
                    printf("tftp: %d KB", reported_transferred >> 10);
                if(tftp->timeouts)
                    printf(" (%d timeouts)", tftp->timeouts);
                printf("\n");
            }
        }

        if(tftp->success){
            printf("Transfer success.\n");
            taken = gogoboot_read_timer() - start;
            taken /= (TIMER_HZ/10); // taken is now in 10ths of a second
            if(taken == 0)
                taken = 1; // avoid div 0
            rate = ((tftp->bytes_transferred / taken)*8) / 1000;
            printf("Transferred %d bytes in %ld.%lds (%ld.%02ld Mbit/sec)\n",
                    tftp->bytes_transferred, taken/10, taken%10, rate/100, rate%100);
        }else{
            printf("Transfer FAILED!\n");
        }

        // close the file
        f_close(&tftp->disk_file);

        // unregister the sink
        net_remove_packet_sink(sink);
    }

    packet_sink_free(sink);
    free(tftp->tftp_filename);
    free(tftp->disk_filename);
    packet_queue_drain(&tftp->data_queue);
    free(tftp);

    return true;
}
