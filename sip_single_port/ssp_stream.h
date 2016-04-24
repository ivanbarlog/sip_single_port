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
#include "ssp_replace.h"


/**
 * Structure that holds information about
 * media streams parsed from SDP of SIP message
 *
 * Basically it's a linked list containing
 * RTP and RTCP ports for each media stream type
 */
typedef struct endpoint_stream {
    /** tells us if the stream is temporary **/
    int temporary;

    char *media;
    char *port;
    char *rtcp_port;

    struct endpoint_stream *next;

} endpoint_stream_t;

/**
 * Frees memory allocated by endpoint streams
 */
void destroy_endpoint_streams(endpoint_stream_t **streams);

/**
 * Parses streams from sdp_info
 * Function is used in parse_endpoint and streams should point
 * directly to endpoint_t streams field
 *
 * Returns 0 on success, -1 otherwise
 * If parsing fails streams is set to NULL
 */
int parse_streams(sip_msg_t *msg, endpoint_stream_t **streams);

/**
 * Returns string containing formatted endpoint_streams_t structure
 * which can be used with LM_* macros
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
int get_stream_type(endpoint_stream_t *streams, unsigned short port, char **type);

/**
 * Returns stream port by specified type
 * used to pair request/response ports
 */
int get_stream_port(endpoint_stream_t *streams, char *type, unsigned short *port);

int get_stream_type_rtcp(endpoint_stream_t *streams, unsigned short src_port, char **type);

int get_stream_rtcp_port(endpoint_stream_t *streams, char *type, unsigned short *port);

/**
 * Checks if the port is present in endpoint streams
 */
int contain_port(endpoint_stream_t *streams, unsigned short *port);

#endif //KAMAILIO_SSP_STREAM_H
