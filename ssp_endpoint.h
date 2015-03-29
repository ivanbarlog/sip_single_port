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

typedef struct endpoint;

endpoint * request_ep = NULL;
endpoint * reply_ep = NULL;

int parseEndpoint(struct sip_msg *msg, endpoint *endpoint);

void printEndpoint(endpoint *endpoint);

#endif //_KAMAILIO_SSP_ENDPOINT_H_
