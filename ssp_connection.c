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

int create_connection(str call_id, connection_t *connection) {
    connection = NULL;
    connection = pkg_malloc(sizeof(connection_t));

    if (connection == NULL) {
        ERR("cannot allocate pkg memory");
        return -1;
    }

    connection->call_id = (str *) pkg_malloc(sizeof(char) * call_id.len);

    if (&(connection->call_id) == NULL) {
        ERR("cannot allocate pkg memory");
        return -1;
    }

    connection->call_id->s = call_id.s;
    connection->call_id->len = call_id.len;

    connection->next = NULL;
    connection->prev = NULL;
    connection->request_endpoint = NULL;
    connection->response_endpoint = NULL;

    return 0;
}

int push_connection(connection_t *connection) {
    int ctr = 1;
    connection_t *tmp;
    tmp = connection;

    tmp->prev = NULL;
    tmp->next = NULL;

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
            connection->request_endpoint_ip ? connection->request_endpoint_ip : "none",
            connection->response_endpoint_ip ? connection->response_endpoint_ip : "none"
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    request_endpoint_info = print_endpoint(connection->request_endpoint);
    response_endpoint_info = print_endpoint(connection->response_endpoint);

    success = asprintf(
            &result,
            "Connection:\n\t%s\nRequest endpoint:\n\t%s\nResponse endpoint:\n\t%s\n\n",
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
    char *result = 0;
    char *placeholder;
    int success;

    connection_t *current;
    current = connections;

    while (current->next != NULL) {
        /**
         * move placeholder to the end of result string
         */
        placeholder = result + strlen(result);

        success = asprintf(
                &placeholder,
                "%s",
                print_connection(current)
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }

        current = current->next;
    }

    return result;
}

int find_connection_by_call_id(str call_id, connection_t *connection) {
    connection = NULL;

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = connections;

    while (current->next != NULL) {
        if (STR_EQ(*(current->call_id), call_id) == 1) {
            connection = current;
            return 0;
        }

        current = current->next;
    }

    INFO("call id '%.*s' was not found in connections list\n", call_id.len, call_id.s);
    return -1;
}

int find_counter_endpoint_by_ip(const char *ip, endpoint_t *endpoint) {
    endpoint = NULL;

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = connections;

    while (current->next != NULL) {
        /*
         * Connection must have both request and response endpoints
         */
        if (has_request_and_response_endpoints(current) == 0) {
            if (strcmp(current->request_endpoint->ip, ip) == 0) {
                endpoint = current->response_endpoint;
                return 0;
            }

            if (strcmp(current->response_endpoint->ip, ip) == 0) {
                endpoint = current->request_endpoint;
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

    if (find_connection_by_call_id(call_id, connection) == 0) {
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
