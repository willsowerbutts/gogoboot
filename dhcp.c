#include <q40types.h>
#include <stdlib.h>
#include "q40hw.h"
#include "cli.h"
#include "net.h"

typedef enum { // start in DHCP_INIT
    DHCP_DISCOVER, // create and send a DISCOVER message, start timer, go to SELECTING
    DHCP_SELECT,   // wait for timer, pick best offer, send REQUEST, go to REQUESTING, if no offers go to INIT
    DHCP_REQUEST,  // receive ACK, start timer, go to BOUND
                   // receive NACK, go to INIT
                   // timeout, go to INIT
    DHCP_BOUND,    // wait for timer, go to RENEWING
    DHCP_RENEW,    // periodically send REQUESTs; transitions as per REQUESTING
} dhcp_state_t;

static dhcp_state_t dhcp_state;
static timer_t dhcp_timer;
static int retransmits;

static void dhcp_enter_state(dhcp_state_t state)
{
    dhcp_state = state;
    retransmits = 0;

    switch(state){
        case DHCP_DISCOVER:
            // construct a DISCOVER message and send it
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
