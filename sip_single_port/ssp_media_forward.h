#ifndef KAMAILIO_SSP_MEDIA_FORWARD_H
#define KAMAILIO_SSP_MEDIA_FORWARD_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../str.h"
#include "../../globals.h" // contains bind_address
#include "../../ip_addr.h"
#include "ssp_endpoint.h"


/**
 * Sends buffer data to specified socket address
 * Returns 0 on success, -1 otherwise
 */
int send_packet_to_endpoint(str *buffer, struct sockaddr_in dst_ip);

/**
 * Fills in source IP and port from receive_info
 */
void set_src_ip_and_port(char *ip, unsigned short *port, struct receive_info *ri);

/**
 * Single-proxy mode - find destination port based on message type
 */
int spm_find_dst_port(int msg_type, endpoint_t *src_endpoint, endpoint_t *dst_endpoint, unsigned short src_port, unsigned short *dst_port);


#endif //KAMAILIO_SSP_MEDIA_FORWARD_H
