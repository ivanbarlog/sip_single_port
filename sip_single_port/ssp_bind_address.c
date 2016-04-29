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

int add_receiving_bind_address(endpoint_t *endpoint, struct socket_info *bind_address)
{
    return 0;
}

int add_sending_bind_address(endpoint_t *endpoint, struct socket_info *bind_address)
{
    return 0;
}

int swap_receiving_bind_address(endpoint_t *endpoint)
{
    return 0;
}

int swap_sending_bind_address(endpoint_t *endpoint)
{
    return 0;
}