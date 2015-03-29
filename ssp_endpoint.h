#ifndef _KAMAILIO_SSP_ENDPOINT_H_
#define _KAMAILIO_SSP_ENDPOINT_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <bits/stdio2.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_methods.h"


#include "../../mem/mem.h"
#include "../../globals.h"

#include "../../re.h"

/*
MSG_BODY_SDP
application/sdp
*/
#include "../../parser/sdp/sdp.h"
/*
int extract_media_attr(str *body, str *mediamedia, str *mediaport, str *mediatransport, str *mediapayload, int *is_rtp);
int extract_rtcp(str *body, str *rtcp);
*/
#include "../../parser/sdp/sdp_helpr_funcs.h"
#include "../cdp_avp/avp_add.h"
#include "../cdp_avp/avp_new.h"
#include "../../msg_translator.h"
#include "../../data_lump.h"

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
