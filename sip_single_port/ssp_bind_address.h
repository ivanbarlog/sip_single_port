#ifndef KAMAILIO_SSP_BIND_ADDRESS_H
#define KAMAILIO_SSP_BIND_ADDRESS_H

#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/parse_content.h"
#include "ssp_endpoint.h"

void print_socket_addresses(struct socket_info *socket);

/**
 * Finds binding socket within kamailio's listening sockets
 * by IP and port and returns pointer to it
 */
struct socket_info *get_bind_address(str address_str, str port_no_str, struct socket_info **list);


#endif //KAMAILIO_SSP_BIND_ADDRESS_H
