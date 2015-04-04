#ifndef _KAMAILIO_SSP_ENDPOINT_H_
#define _KAMAILIO_SSP_ENDPOINT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "../../dprint.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/msg_parser.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"

#include "ssp_funcs.h"
#include "ssp_body.h"

/** endpoint structure */
typedef struct endpoint
{
    replacedPort *rtp_ports;

    unsigned short rtp_port;
    unsigned short rtcp_port;
    char ip [50];

    /* usable in UDP socket communication */
    struct sockaddr_in ip_address;

    /* SIP_REQ or SIP_REP */
    unsigned short type;

    struct endpoint *next;

} endpoint;


int parseEndpoint(struct sip_msg *msg, struct endpoint *endpoint);

void printEndpoint(struct endpoint *endpoint);

#endif //_KAMAILIO_SSP_ENDPOINT_H_
