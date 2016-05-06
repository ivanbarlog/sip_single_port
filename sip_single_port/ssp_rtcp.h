#ifndef KAMAILIO_SSP_RTCP_H
#define KAMAILIO_SSP_RTCP_H

#include <stdlib.h>

#include "../../ip_addr.h"
#include "ssp_connection.h"
#include "ssp_bind_address.h"

/**
 * Parses `fraction lost` from RTCP packet and compares it to threshold
 * If `fraction lost` exceeds 1 is returned otherwise 0 is returned
 */
int exceeds_limit(char *rtcp, int threshold);

/**
 * Sends notification to client's kamailio defined by ip:port
 * Port to which client's kamailio will be switched is determined by traversing
 * connections_list and finding count of connections with sending socket; after
 * that the socket_list is traversed and we are searching the least occupied socket
 */
int notify(char *ip, unsigned short port, socket_list_t *socket_list, struct socket_info *current_socket);

#endif //KAMAILIO_SSP_RTCP_H
