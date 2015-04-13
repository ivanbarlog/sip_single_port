#ifndef KAMAILIO_SSP_CONNECTION_H
#define KAMAILIO_SSP_CONNECTION_H

#define _GNU_SOURCE //allows us to use asprintf

#include <stdio.h>
#include "../../str.h"
#include "../../mem/mem.h"

#include "ssp_endpoint.h"

typedef struct connection {
    char *call_id_raw;
    str *call_id;
    char *request_endpoint_ip;
    char *response_endpoint_ip;

    endpoint_t *request_endpoint;
    endpoint_t *response_endpoint;

    struct connection *prev;
    struct connection *next;

} connection_t;


/**
 * Initializes empty connection structure and assign
 * the call_id to it
 *
 * Returns 0 on success, -1 otherwise
 * On failure the connection is set to NULL
 */
connection_t *create_connection(str call_id);

/**
 * Pushes connection to connections list
 * Returns actual count of connections in connections list
 */
int push_connection(connection_t *connection);

/**
 * Returns string containing formatted connection structure
 * which can be used with LM_* macros
 *
 * On error returns NULL
 */
char *print_connection(connection_t *connection);

/**
 * Returns string containing formatted connections list structure
 * which can be used with LM_* macros
 *
 * On error returns NULL
 */
char *print_connections_list();

/**
 * Searches for connection by call_id
 * Returns 0 on success, -1 on fail
 * If connection is not found *connection is set to NULL
 */
int find_connection_by_call_id(str call_id, connection_t **connection);

/**
 * Searches for connections where request_endpoint_ip or response_endpoint_ip
 * is equal to ip
 * When connection is found function returns 0 and endpoint is set to
 * counter part endpoint eg. if request_endpoint_ip equals ip endpoint
 * is set to response_endpoint and vice versa
 */
int find_counter_endpoint_by_ip(const char *ip, endpoint_t **endpoint);

/**
 * Finds connection by call_id and removes it from connections list
 * Returns 0 on success otherwise -1
 */
int remove_connection(str call_id);

#endif //KAMAILIO_SSP_CONNECTION_H
