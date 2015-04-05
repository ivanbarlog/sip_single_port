#ifndef _KAMAILIO_SSP_ENDPOINT_H_
#define _KAMAILIO_SSP_ENDPOINT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../dprint.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/msg_parser.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"

#include "ssp_funcs.h"
#include "ssp_body.h"

typedef struct endpoint_stream
{
    str media;
    str port;
    str rtcp_port;

    struct endpoint_stream *prev;
    struct endpoint_stream *next;
} endpoint_stream_t;

//typedef enum {SIP_REQ, SIP_REP} endpointType;

/** endpoint structure */
typedef struct endpoint
{
    str *call_id;

    // obsolete = replaced by streams
    // todo: remove
    unsigned short rtp_port;

    endpoint_stream_t *streams;
    unsigned short rtcp_port;
    char ip [50];

    /* usable in UDP socket communication */
    struct sockaddr_in ip_address;

    /* SIP_REQ or SIP_REP */
    //todo: change to endpointType
    unsigned short type;

    struct endpoint *prev;
    struct endpoint *next;

} endpoint_t;

int parseEndpoint(struct sip_msg *msg, endpoint_t *endpoint, int msg_type);

void printEndpoint(endpoint_t *endpoint);

int initList();

void pushEndpoint(endpoint_t *endpoint);

int findEndpoint(const char * ip, endpoint_t *endpoint);

int removeEndpoint(const char *ip);

int endpointExists(const char *ip, int type);

int keyCmp(const char *key, const char *value);

void printEndpointStreams(endpoint_stream_t *head);

struct sockaddr_in* getStreamAddress(endpoint_t *endpoint, const char *streamType);

int findStream(endpoint_stream_t *head, endpoint_stream_t *stream, unsigned short port);

#endif //_KAMAILIO_SSP_ENDPOINT_H_
