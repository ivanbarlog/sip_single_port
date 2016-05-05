#include "ssp_bind_address.h"

socket_item_t *find_least_used_socket(socket_list_t *socket_list) {

    if (socket_list == NULL || socket_list->head == NULL) {
        ERR("socket list not initialized.\n");
        return NULL;
    }

    socket_item_t *current = socket_list->head;
    socket_item_t *least_used = current;

    print_socket_list(socket_list);

    while (current != NULL) {

        INFO("%d > %d\n", least_used->count, current->count);

        // set current as least_used if it has less clients subscribed
        if (least_used->count > current->count) {
            least_used = current;
        }

        INFO("least_used: %d\n", least_used->count);

        // we stop search if the least_used count is 0
        if (least_used->count == 0) {
            break;
        }

        current = current->next;
    }

    return least_used;
}

socket_item_t *init_socket_list(struct socket_info *socket_list) {

    struct socket_info *socket = socket_list;

    socket_item_t *current = NULL;
    socket_item_t *prev = NULL;
    socket_item_t *list = NULL;

    while (socket != NULL) {
        current = (socket_item_t *) shm_malloc(sizeof(socket_item_t));
        current->count = 0;
        current->socket = socket;
        current->next = NULL;

        if (list == NULL) {
            list = current;
        }

        if (prev != NULL) {
            prev->next = current;
        }

        prev = current;
        current = NULL;

        socket = socket->next;
    }

    return list;
}

static socket_item_t *find_socket_item(struct socket_info *socket, socket_list_t *socket_list) {

    socket_item_t *current = socket_list->head;

    while (current) {
        if (current->socket == socket) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

int increment_clients_count(struct socket_info *socket, socket_list_t *socket_list) {

    socket_item_t *item = find_socket_item(socket, socket_list);

    if (item == NULL) {
        ERR("Cannot find socket item in the list\n");
        return -1;
    }

    lock_socket_list(socket_list);
    item->count++;
    unlock_socket_list(socket_list);

    return 0;
}

int decrement_clients_count(struct socket_info *socket, socket_list_t *socket_list) {

    socket_item_t *item = find_socket_item(socket, socket_list);

    if (item == NULL) {
        ERR("Cannot find socket item in the list\n");
        return -1;
    }

    lock_socket_list(socket_list);
    item->count--;
    unlock_socket_list(socket_list);

    return 0;
}

void lock_socket_list(socket_list_t *list) {
    lock_get(list->lock);
}

void unlock_socket_list(socket_list_t *list) {
    lock_release(list->lock);
}

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

int print_socket_list(socket_list_t *list) {
    if (list->head == NULL) {
        ERR("socket list is not initialized\n");
        return -1;
    }

    INFO("kamailio's subscribers count:\n");

    socket_item_t *current = list->head;
    while (current != NULL) {

        INFO(
                "%.*s:%d: %d\n",
                current->socket->address_str.len,
                current->socket->address_str.s,
                current->socket->port_no,
                current->count
        );

        current = current->next;
    }

    return 0;
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
