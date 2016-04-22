#ifndef KAMAILIO_SSP_BIND_ADDRESS_H
#define KAMAILIO_SSP_BIND_ADDRESS_H

#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/parse_content.h"

/**
 * Finds binding socket within kamailio's listening sockets
 * by IP and port and returns pointer to it
 */
struct socket_info *get_bind_address(str address_str, str port_no_str, struct socket_info **list);

/**
 * Adds binding address to endpoint
 * as temporary receiving address
 */
int add_receiving_bind_address(endpoint_t *endpoint, struct socket_info *bind_address);

/**
 * Adds binding address to endpoint
 * as temporary sending address
 */
int add_sending_bind_address(endpoint_t *endpoint, struct socket_info *bind_address);

/**
 * Removes current receiving binding address and replaces
 * it with temporary one
 */
int swap_receiving_bind_address(endpoint_t *endpoint);

/**
 * Removes current sending binding address and replaces
 * it with temporary one
 */
int swap_sending_bind_address(endpoint_t *endpoint);

#endif //KAMAILIO_SSP_BIND_ADDRESS_H
