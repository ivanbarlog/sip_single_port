#ifndef KAMAILIO_SSP_CONNECTION_H
#define KAMAILIO_SSP_CONNECTION_H

#define _GNU_SOURCE //allows us to use asprintf

#include <stdio.h>
#include "../../str.h"
#include "../../mem/mem.h"

#include "ssp_endpoint.h"

typedef struct alias {
    char *ip_port;
    struct alias *next;
} alias_t;

typedef struct connection {
    char *call_id_raw;
    str *call_id;
    char *request_endpoint_ip;
    char *response_endpoint_ip;
    int same_ip; // if request_endpoint_ip equals response_endpoint_ip this is set to 1 otherwise 0
    alias_t *req_ip_alias; // Request IP aliases - format IP:port
    alias_t *res_ip_alias; // Response IP aliases - format IP:port

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
 * Searches provided connection for endpoint with provided IP,
 * then searches counter part endpoint for provided media_type and fills in port with the destination port
 * Returns 0 on success, -1 on fail
 * If port is not found *port is set to NULL
 */
int get_counter_port(const char *ip, str type, connection_t *connection, unsigned short *port);

/**
 * Searches for connections where request_endpoint_ip or response_endpoint_ip
 * is equal to ip or when same_ip is set to 1 it searches in aliases for IP:port
 * When connection is found function returns 0 and endpoint is set to
 * counter part endpoint eg. if request_endpoint_ip equals ip endpoint
 * is set to response_endpoint and vice versa
 */
int find_counter_endpoint(const char *ip, short unsigned int port, endpoint_t **endpoint);

/**
 * If ip_port is in aliases function returns 1 otherwise -1
 */
int find_endpoint_by_alias(alias_t *aliases, char *ip_port);

/**
 * Finds connection by call_id and removes it from connections list
 * Returns 0 on success otherwise -1
 */
int remove_connection(str call_id);

/**
 * Fills in aliases (IP:port) for both connection's endpoints
 * Returns 0 on success otherwise -1
 */
int fill_in_aliases(connection_t *connection);

/**
 * Creates IP:port alias
 */
alias_t *create_alias(char *ip, char *port);

/**
 * Adds aliases to linked list
 */
int add_aliases(char *ip, endpoint_t *endpoint, alias_t **aliases);

/**
 * Prints aliases for debugging purposes
 */
char *print_endpoint_aliases(alias_t *alias);

#endif //KAMAILIO_SSP_CONNECTION_H
