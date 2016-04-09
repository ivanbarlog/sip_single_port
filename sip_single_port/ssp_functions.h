#ifndef KAMAILIO_SSP_FUNCTIONS_H
#define KAMAILIO_SSP_FUNCTIONS_H

#define BIT7 0x80
#define BIT6 0x40
#define BIT5 0x20

#define _GNU_SOURCE //allows us to use asprintf

#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/parse_content.h"

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
int get_socket_addr(char *endpoint_ip, unsigned short port, struct sockaddr_in **tmp);

/**
 * Parses Call-ID from message
 * Returns 0 on success, -1 otherwise
 */
int parse_call_id(sip_msg_t *msg, str *call_id);

/**
 * Checks wether message contains SDP body
 */
int get_msg_body(struct sip_msg *msg, str *body);

/**
 * Copies string from original_string to newly allocated shm memory
 */
int shm_copy_string(const char *original_string, int original_length, char **new_string);

/**
 * Copies string from original_string to newly allocated pkg memory
 */
int pkg_copy_string(const char *original_string, int original_length, char **new_string);

/**
 * Returns hex representation string of the bytes in str
 */
char *print_hex_str(str *str);

#endif //KAMAILIO_SSP_FUNCTIONS_H
