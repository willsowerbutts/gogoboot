/* Copyright (C) 2015-2023 William R. Sowerbutts */

#include <types.h>
#include <stdlib.h>
#include <tinyalloc.h>
#include <cli.h>
#include <net.h>

void do_tftp(char *argv[], int argc)
{
    const char *server=NULL, *src, *dst;
    uint32_t targetip = 0;

    // this needs some improvements to make it more user friendly
    // right now it expects the user to know too much
    
    switch(argc){
        case 1:
            src = dst = argv[0];
            break;
        case 2:
            src = argv[0];
            dst = argv[1];
            break;
        case 3:
            server = argv[0];
            src = argv[1];
            dst = argv[2];
            break;
        default:
            printf("Unexpected number of arguments\n");
            return;
    }

    if(!server)
        server = get_environment_variable("tftp_server");

    if(!server){
        printf("please specify the server IP address (or 'set tftp_server <ip>')\n");
        return;
    }

    if(server){
        targetip = net_parse_ipv4(server);
        if(targetip == 0){
            printf("Cannot parse server IPv4 address \"%s\"\n", server);
            return;
        }
    }

    tftp_receive(targetip, src, dst);
}
