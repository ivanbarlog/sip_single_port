#ifndef _KAMAILIO_SSP_ENDPOINT_H_
#define _KAMAILIO_SSP_ENDPOINT_H_

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "../../dprint.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/msg_parser.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"

#include "ssp_funcs.h"

int printed = 0;

endpoint * request_ep = NULL;
endpoint * reply_ep = NULL;

/** endpoint structure */
typedef struct endpoint
{
    unsigned short rtp_port;
    unsigned short rtcp_port;
    char ip [50];

    /* usable in UDP socket communication */
    struct sockaddr_in ip_address;

    /* SIP_REQ or SIP_REP */
    unsigned short type;

    struct endpoint *next;

} endpoint;

int parseEndpoint(struct sip_msg *msg, endpoint *endpoint);

void printEndpoint(endpoint *endpoint);

#endif //_KAMAILIO_SSP_ENDPOINT_H_
