#include "ssp_connection.h"

/**
 * Head of connections list
 */
static connection_t *connections = NULL;

static int has_request_and_response_endpoints(connection_t *connection) {
    if (connection->request_endpoint == NULL) {
        return -1;
    }

    if (connection->response_endpoint == NULL) {
        return -1;
    }

    return 0;
}

connection_t *create_connection(str call_id) {
    connection_t *connection;
    connection = pkg_malloc(sizeof(connection_t));

    if (connection == NULL) {
        ERR("cannot allocate pkg memory");
        return NULL;
    }


    if (copy_str(&call_id, &(connection->call_id_raw), &(connection->call_id)) == -1) {
        ERR("cannot allocate memory.\n");
        return NULL;
    }

    connection->next = NULL;
    connection->prev = NULL;
    connection->request_endpoint = NULL;
    connection->response_endpoint = NULL;
    connection->request_endpoint_ip = NULL;
    connection->response_endpoint_ip = NULL;

    return connection;
}

int push_connection(connection_t *connection) {
    int ctr = 1;
    connection_t *tmp;
    tmp = connection;

    /**
     * If connections wasn't initialized yet
     * connection became root of connections
     */
    if (connections == NULL) {
        connections = tmp;
        return ctr;
    }

    connection_t *current;
    current = connections;

    /**
     * Find last connection in list
     */
    while (current->next != NULL) {
        current = current->next;
        ctr++;
    }

    /**
     * Set link between last connection
     * and the one we are trying to push
     */
    tmp->prev = current;
    current->next = tmp;

    return ctr;
}

char *print_connection(connection_t *connection) {
    char *result;
    char *connection_info;
    char *request_endpoint_info;
    char *response_endpoint_info;
    int success;

    success = asprintf(
            &connection_info,
            "Call-ID: %.*s\nRequest IP: %s\nResponse IP: %s\n",
            connection->call_id->len, connection->call_id->s,
            connection->request_endpoint_ip != NULL ? connection->request_endpoint_ip : "none",
            connection->response_endpoint_ip != NULL ? connection->response_endpoint_ip : "none"
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    request_endpoint_info = print_endpoint(connection->request_endpoint);
    response_endpoint_info = print_endpoint(connection->response_endpoint);

    success = asprintf(
            &result,
            "##########\nConnection:\n%s\n#####\nRequest endpoint:\n%s\n#####\nResponse endpoint:\n%s\n\n",
            connection_info,
            request_endpoint_info,
            response_endpoint_info
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_connections_list() {

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return NULL;
    }

    char *result = 0;
    int success;

    connection_t *current;
    current = connections;

    result = print_connection(current);
    while (current->next != NULL) {
        success = asprintf(
                &result,
                "%s%s",
                result, print_connection(current->next)
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }

        current = current->next;
    }

    return result;
}

int find_connection_by_call_id(str call_id, connection_t **connection) {
    *connection = NULL;

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = connections;

    while (current != NULL) {
        if (STR_EQ(*(current->call_id), call_id) == 1) {
            *connection = current;
            return 0;
        }

        current = current->next;
    }

    INFO("call id '%.*s' was not found in connections list\n", call_id.len, call_id.s);
    return -1;
}

int find_counter_endpoint_by_ip(const char *ip, endpoint_t **endpoint) {
    *endpoint = NULL;

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = connections;

    while (current != NULL) {
        /*
         * Connection must have both request and response endpoints
         */
        if (has_request_and_response_endpoints(current) == 0) {
            if (strcmp(current->request_endpoint->ip, ip) == 0) {
                *endpoint = current->response_endpoint;
                if (current->response_endpoint->sibling == NULL) {
                    current->response_endpoint->sibling = current->request_endpoint;
                }

                return 0;
            }

            if (strcmp(current->response_endpoint->ip, ip) == 0) {
                *endpoint = current->request_endpoint;
                if (current->request_endpoint->sibling == NULL) {
                    current->request_endpoint->sibling = current->response_endpoint;
                }

                return 0;
            }
        }

        current = current->next;
    }

    INFO("IP '%s' was not found in connections\n", ip);
    return -1;
}

int remove_connection(str call_id) {
    connection_t *prev;
    connection_t *next;
    connection_t *connection = NULL;

    if (find_connection_by_call_id(call_id, &connection) == 0) {
        prev = connection->prev;
        next = connection->next;

        if (prev == NULL && next == NULL) {
            connections = NULL;
        } else if (prev == NULL) {
            connections = next;
            next->prev = NULL;
        } else if (next == NULL) {
            prev->next = NULL;
        } else {
            prev->next = next;
            next->prev = prev;
        }

        pkg_free(connection);

        return 0;
    }

    INFO("Connection with '%.*s' call id was not found.\n", call_id.len, call_id.s);
    return -1;
}
