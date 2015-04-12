#ifndef KAMAILIO_SSP_FUNCTIONS_H
#define KAMAILIO_SSP_FUNCTIONS_H

#define BIT6 0x40
#define BIT5 0x20

#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/msg_parser.h"

typedef enum msg_type {
    SSP_SIP_REQUEST,
    SSP_SIP_RESPONSE,
    SSP_RTP_PACKET,
    SSP_RTCP_PACKET,
    SSP_OTHER
} msg_type;

/**
 * Returns message type
 * eg. SIP request, SIP response, RTP, RTCP or other
 */
msg_type get_msg_type(sip_msg_t *msg);

/**
 * Returns sockaddr structure from specified IP and port
 */
int get_socket_addr(char *endpoint_ip, unsigned short port, struct sockaddr_in *ip);

#endif //KAMAILIO_SSP_FUNCTIONS_H
