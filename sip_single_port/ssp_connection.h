#ifndef KAMAILIO_SSP_CONNECTION_H
#define KAMAILIO_SSP_CONNECTION_H

#define _GNU_SOURCE //allows us to use asprintf

#include <stdio.h>
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../locking.h"

#include "ssp_endpoint.h"

typedef struct connection {
    char *call_id;

    char **request_endpoint_ip;
    char **response_endpoint_ip;

    endpoint_t *request_endpoint;
    endpoint_t *response_endpoint;

    struct connection *prev;
    struct connection *next;

    gen_lock_t *lock;

} connection_t;

typedef struct connections_list {
    connection_t *head;
} connections_list_t;

/**
 * Destroys connection by provided pointer
 *
 * Returns nothing
 */
void destroy_connection(connection_t *connection);

/**
 * Initializes empty connection structure and assign
 * the call_id to it
 *
 * Returns 0 on success, -1 otherwise
 * On failure the connection is set to NULL
 */
connection_t *create_connection(char *call_id);

/**
 * Pushes connection to connections list
 * Returns actual count of connections in connections list
 */
int push_connection(connection_t *connection, connection_t **connection_list);

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
char *print_connections_list(connection_t **connection_list);

/**
 * Searches for connection by call_id
 * Returns 0 on success, -1 on fail
 * If connection is not found *connection is set to NULL
 */
int find_connection_by_call_id(char *call_id, connection_t **connection, connection_t **connection_list);

/**
 * Searches provided connection for endpoint with provided IP,
 * then searches counter part endpoint for provided media_type and fills in port with the destination port
 * Returns 0 on success, -1 on fail
 * If port is not found *port is set to NULL
 */
int get_counter_port(const char *ip, char *type, connection_t *connection, unsigned short *port);

/**
 * Searches for connections where request_endpoint_ip or response_endpoint_ip
 * is equal to ip or when same_ip is set to 1 it searches in aliases for IP:port
 * When connection is found function returns 0 and endpoint is set to
 * counter part endpoint eg. if request_endpoint_ip equals ip endpoint
 * is set to response_endpoint and vice versa
 */
int find_counter_endpoint(const char *ip, short unsigned int port, endpoint_t **endpoint, connection_t **connection_list);

/**
 * Finds connection by call_id and removes it from connections list
 * Returns 0 on success otherwise -1
 */
int remove_connection(char *call_id, connection_t **connection_list);

int add_new_in_rule(
        const char *ip,
        short unsigned int port,
        char *new_port,
        connection_t **connection_list
);

int remove_temporary_rules(
        const char *ip,
        short unsigned int port,
        connection_t **connection_list
);

int change_socket_for_endpoints(
        const char *ip,
        short unsigned int port,
        struct socket_info *new_socket,
        connection_t **connection_list
);

void lock_connection(connection_t *connection);
void unlock_connection(connection_t *connection);


#endif //KAMAILIO_SSP_CONNECTION_H
