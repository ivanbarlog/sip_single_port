#ifndef KAMAILIO_SSP_BIND_ADDRESS_H
#define KAMAILIO_SSP_BIND_ADDRESS_H

#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../parser/parse_content.h"
#include "../../locking.h"

#include "ssp_endpoint.h"

typedef struct socket_item {
    int count;
    struct socket_info *socket;

    struct socket_item *next;
} socket_item_t;

typedef struct socket_list {
    socket_item_t *head;
    gen_lock_t *lock;
} socket_list_t;

socket_item_t *init_socket_list(struct socket_info *socket_list);

socket_item_t *find_least_used_socket(socket_list_t *socket_list);

int increment_clients_count(struct socket_info *socket, socket_list_t *socket_list);

int decrement_clients_count(struct socket_info *socket, socket_list_t *socket_list);

void lock_socket_list(socket_list_t *list);

void unlock_socket_list(socket_list_t *list);

void print_socket_addresses(struct socket_info *socket);

int print_socket_list(socket_list_t *list);

/**
 * Finds binding socket within kamailio's listening sockets
 * by IP and port and returns pointer to it
 */
struct socket_info *get_bind_address(str address_str, str port_no_str, struct socket_info **list);

#endif //KAMAILIO_SSP_BIND_ADDRESS_H
