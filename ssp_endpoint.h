#ifndef KAMAILIO_SSP_ENDPOINT_H
#define KAMAILIO_SSP_ENDPOINT_H

#define _GNU_SOURCE //allows us to use asprintf

#include <stdio.h>
#include "../../str.h"

#include <netinet/in.h>
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../parser/sdp/sdp.h"
#include "../../mem/mem.h"

#include "ssp_stream.h"
#include "ssp_functions.h"

/**
 * Structure that holds IP of endpoint which
 * is present in SIP dialog
 * It also holds linked list of media streams
 * which are transported from endpoint
 */
typedef struct endpoint {
    char *ip;

    /* list of all media streams */
    struct endpoint_stream *streams;

    struct endpoint *sibling;

} endpoint_t;

/**
 * Parses endpoint from sip_msg structure
 * Returns 0 on success, -1 otherwise
 * If parsing fails endpoint is set to NULL
 */
endpoint_t *parse_endpoint(sip_msg_t *msg);

/**
 * Returns string containing formatted endpoint structure
 * which can be used with LM_* macros
 */
char *print_endpoint(endpoint_t *endpoint);

#endif //KAMAILIO_SSP_ENDPOINT_H
