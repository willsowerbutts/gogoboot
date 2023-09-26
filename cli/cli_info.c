/* Copyright (C) 2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <cli.h>
#include <net.h>
#include <init.h>
#include <tinyalloc.h>

static void help_cmd_table(const cmd_entry_t *cmd)
{
    while (cmd->name) {
        printf("%12s : %s\n", cmd->name, cmd->helpme);
        cmd++;
    }
}

void help(char *argv[], int argc)
{
    help_cmd_table(builtin_cmd_table);
    help_cmd_table(target_cmd_table);
}

void do_meminfo(char *argv[], int argc)
{
    report_memory_layout();
    printf("internal heap (tinyalloc):\nfresh blocks: %ld\nfree blocks: %ld\nused blocks: %ld\nalloc bytes: %ld\n",
            ta_num_fresh(), ta_num_free(), ta_num_used(), ta_bytes_used());
    printf("ta_check %s\n", ta_check() ? "ok" : "FAILED");
}

void do_netinfo(char *argv[], int argc)
{
    int prefixlen = 0;
    uint32_t mask = interface_subnet_mask;
    while(mask){
        prefixlen++;
        mask <<= 1;
    }

    printf("IPv4 address: %d.%d.%d.%d/%d\n", 
            (int)(interface_ipv4_address >> 24 & 0xff),
            (int)(interface_ipv4_address >> 16 & 0xff),
            (int)(interface_ipv4_address >>  8 & 0xff),
            (int)(interface_ipv4_address       & 0xff),
            prefixlen);
    printf("Gateway: %d.%d.%d.%d\n", 
            (int)(interface_ipv4_gateway >> 24 & 0xff),
            (int)(interface_ipv4_gateway >> 16 & 0xff),
            (int)(interface_ipv4_gateway >>  8 & 0xff),
            (int)(interface_ipv4_gateway       & 0xff));
    printf("DNS server: %d.%d.%d.%d\n", 
            (int)(interface_dns_server >> 24 & 0xff),
            (int)(interface_dns_server >> 16 & 0xff),
            (int)(interface_dns_server >>  8 & 0xff),
            (int)(interface_dns_server       & 0xff));

    printf("packet_rx_count %ld\n", packet_rx_count);
    printf("packet_tx_count %ld\n", packet_tx_count);
    printf("packet_alive_count %ld\n", packet_alive_count);
    printf("packet_discard_count %ld\n", packet_discard_count);
    printf("packet_bad_cksum_count %ld\n", packet_bad_cksum_count);

    net_dump_packet_sinks();
}
