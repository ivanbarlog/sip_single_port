#ifndef KAMAILIO_SSP_MEDIA_FORWARD_H
#define KAMAILIO_SSP_MEDIA_FORWARD_H

#define _GNU_SOURCE //allows us to use asprintf

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
 * Finds destination port based on message type and source port
 */
int find_dst_port(int msg_type, endpoint_t *src_endpoint, endpoint_t *dst_endpoint, unsigned short src_port, unsigned short *dst_port, char **media_type);

/**
 * Parses tagged RTP/RTCP message and fills in call_id and media_type
 */
int parse_tagged_msg(const char *msg, char **call_id, char **media_type, int *tag_length);

/**
 * Adds tag 'Call-ID:Media-type' to RTP/RTCP message
 */
int tag_message(str *obuf, char *call_id, char *media_type);

/**
 * Removes tag from message and creates original message
 */
int remove_tag(str *obuf, int tag_length);

#endif //KAMAILIO_SSP_MEDIA_FORWARD_H
