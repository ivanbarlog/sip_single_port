#ifndef KAMAILIO_SSP_STREAM_H
#define KAMAILIO_SSP_STREAM_H

#define _GNU_SOURCE //allows us to use asprintf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "../../str.h"
#include "../../parser/sdp/sdp.h"

#include "ssp_endpoint.h"
#include "ssp_functions.h"


/**
 * Structure that holds information about
 * media streams parsed from SDP of SIP message
 *
 * Basically it's a linked list containing
 * RTP and RTCP ports for each media stream type
 */
typedef struct endpoint_stream {
    char *media_raw;
    str *media;
    char *port_raw;
    str *port;
    char *rtcp_port_raw;
    str *rtcp_port;

    struct endpoint_stream *next;

} endpoint_stream_t;

/**
 * Parses streams from sdp_info
 * Function is used in parse_endpoint and streams should point
 * directly to endpoint_t streams field
 *
 * Returns 0 on success, -1 otherwise
 * If parsing fails streams is set to NULL
 */
int parse_streams(sdp_info_t *sdp_info, endpoint_stream_t **streams);

/**
 * Returns socket address from streams list (provided by endpoint)
 * based on RTP or RTCP port
 *
 * Returns 0 on success, -1 otherwise
 * If stream is not found ip is set to NULL
 *
 * todo: not implemented yet - should be removed and replaced by get_stream_type() and get_stream_port()
 */
int get_stream_ip(endpoint_stream_t *streams, unsigned short port, struct sockaddr_in *ip);

/**
 * Returns string containing formatted endpoint_streams_t structure
 * which can be used with LM_* macros
 *
 * todo: should be static
 */
char *print_stream(endpoint_stream_t *stream);

/**
 * Traverses all endpoint streams and returns printable string
 * Internally uses print_stream function
 */
char *print_endpoint_streams(endpoint_stream_t *streams);

/**
 * Returns stream type by specified port
 * used to pair request/response ports
 */
int get_stream_type(endpoint_stream_t *streams, unsigned short port, str **type);

/**
 * Returns stream port by specified type
 * used to pair request/response ports
 */
int get_stream_port(endpoint_stream_t *streams, str type, unsigned short *port);


#endif //KAMAILIO_SSP_STREAM_H
