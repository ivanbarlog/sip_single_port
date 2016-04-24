#include "ssp_bind_address.h"

void print_socket_addresses(struct socket_info *socket)
{
    INFO("kamailio listens on these adresses:\n");

    struct socket_info *current_bind_address = socket;
    while (current_bind_address != NULL) {

        INFO(
                "%.*s:%d\n",
                current_bind_address->address_str.len,
                current_bind_address->address_str.s,
                current_bind_address->port_no
        );

        current_bind_address = current_bind_address->next;
    }
}

struct socket_info *get_bind_address(str address_str, str port_no_str, struct socket_info **list)
{
    struct socket_info *current_bind_address = *list;
    while (current_bind_address != NULL) {
        if (STR_EQ(address_str, current_bind_address->address_str) &&
            STR_EQ(port_no_str, current_bind_address->port_no_str))
        {
            return current_bind_address;
        }

        current_bind_address = current_bind_address->next;
    }

    return NULL;
}

int add_receiving_bind_address(endpoint_t *endpoint, struct socket_info *bind_address) {
    if (endpoint->tmp_receiving_socket != NULL) {
        ERR("Temporary receiving socket already set.\n");

        return -1;
    }

    endpoint->tmp_receiving_socket = bind_address;

    return 0;
}

int add_sending_bind_address(endpoint_t *endpoint, struct socket_info *bind_address)
{
    if (endpoint->tmp_sending_socket != NULL) {
        ERR("Temporary sending socket already set.\n");

        return -1;
    }

    endpoint->tmp_sending_socket = bind_address;

    return 0;
}

int swap_receiving_bind_address(endpoint_t *endpoint)
{
    if (endpoint->tmp_receiving_socket == NULL) {
        ERR("Temporary receiving socket not set yet.\n");

        return -1;
    }

    endpoint->receiving_socket = endpoint->tmp_receiving_socket;
    endpoint->tmp_receiving_socket = NULL;

    return 0;
}

int swap_sending_bind_address(endpoint_t *endpoint)
{
    if (endpoint->tmp_sending_socket == NULL) {
        ERR("Temporary sending socket not set yet.\n");

        return -1;
    }

    endpoint->sending_socket = endpoint->tmp_sending_socket;
    endpoint->tmp_sending_socket = NULL;

    return 0;
}