#include "ssp_connection.h"

static int has_request_and_response_endpoints(connection_t *connection) {
    if (connection->request_endpoint == NULL) {
        return -1;
    }

    if (connection->response_endpoint == NULL) {
        return -1;
    }

    return 0;
}

void destroy_connection(connection_t *connection) {
    if (connection->lock != NULL)
        lock_dealloc(connection->lock);

    if (connection->call_id != NULL)
        shm_free(connection->call_id);


    if (connection->request_endpoint != NULL)
        destroy_endpoint(connection->request_endpoint);

    if (connection->response_endpoint != NULL)
        destroy_endpoint(connection->response_endpoint);

    // we don't need to free next, prev, request and response endpoint IP
    // since they are just pointers and will be freed after whole connection is

    shm_free(connection);
}

connection_t *create_connection(char *call_id) {
    connection_t *connection = (connection_t *) shm_malloc(sizeof(connection_t));

    if (connection == NULL) {
        ERR("cannot allocate shm memory");
        return NULL;
    }

    connection->next = NULL;
    connection->prev = NULL;

    connection->call_id = NULL;

    connection->request_endpoint = NULL;
    connection->response_endpoint = NULL;

    connection->request_endpoint_ip = NULL;
    connection->response_endpoint_ip = NULL;

    connection->lock = NULL;

    connection->lock = lock_alloc();

    if (connection->lock == NULL)
    {
        ERR("cannot allocate the lock\n");
        destroy_connection(connection);

        return NULL;
    }

    shm_copy_string(call_id, strlen(call_id), &(connection->call_id));

    return connection;
}

int push_connection(connection_t *connection, connection_t **connection_list) {
    int ctr = 1;
    connection_t *tmp;
    tmp = connection;

    /**
     * If connections wasn't initialized yet
     * connection became root of connections
     */
    if (*connection_list == NULL) {
        *connection_list = tmp;
        return ctr;
    }

    connection_t *current;
    current = *connection_list;

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

static char *get_hdr_line(int type) {
    if (type == 0) {
        return " +=========================================+";
    }

    return " +====================+====================+";

}

static char *get_line(int type) {
    if (type == 0) {
        return " +-----------------------------------------+";
    }

    return " +--------------------+--------------------+";
}

char *print_connection(connection_t *connection) {
    char *result = NULL;
    char *connection_info = NULL;
    char *request_endpoint_info = NULL;
    char *response_endpoint_info = NULL;
    int success;

    success = asprintf(
            &connection_info,
            "%s\n | %-39s |\n%s\n | %-39s |\n%s\n | %-18s | %-18s |\n%s\n | %-18s | %-18s |\n%s\n",
            get_hdr_line(0),
            "Connection by Call-ID",
            get_line(0),
            connection->call_id,
            get_hdr_line(1),
            "Request IP",
            "Response IP",
            get_line(1),
            connection->request_endpoint != NULL ? *(connection->request_endpoint_ip) : "none",
            connection->response_endpoint != NULL ? *(connection->response_endpoint_ip) : "none",
            get_line(0)
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    request_endpoint_info = print_endpoint(connection->request_endpoint, "Request endpoint");
    response_endpoint_info = print_endpoint(connection->response_endpoint, "Response endpoint");

    success = asprintf(
            &result,
            "%s\n%s%s\n\n\n",
            connection_info,
            request_endpoint_info,
            response_endpoint_info
    );

    if (connection_info != NULL)
        free(connection_info);

    if (request_endpoint_info != NULL)
        free(request_endpoint_info);

    if (response_endpoint_info != NULL)
        free(response_endpoint_info);

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_connections_list(connection_t **connection_list) {

    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return NULL;
    }

    char *result = 0;
    int success;

    connection_t *current;
    current = *connection_list;

    char *connection_info = NULL;

    result = print_connection(current);
    while (current->next != NULL) {
        connection_info = print_connection(current->next);

        success = asprintf(
                &result,
                "%s%s",
                result, connection_info
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }

        if (connection_info != NULL)
            free(connection_info);

        current = current->next;
    }

    return result;
}

int find_connection_by_call_id(char *call_id, connection_t **connection, connection_t **connection_list) {
    *connection = NULL;

    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    while (current != NULL) {

        if (strcmp(current->call_id, call_id) == 0) {
            *connection = current;

            return 0;
        }

        current = current->next;
    }

    INFO("call id '%s' was not found in connections list\n", call_id);

    return -1;
}

int get_counter_port(const char *ip, char *type, connection_t *connection, unsigned short *port) {

    endpoint_t *counter_endpoint = NULL;

    if (strcmp(*(connection->request_endpoint_ip), ip) == 0) {
        counter_endpoint = connection->response_endpoint;
    }

    if (strcmp(*(connection->response_endpoint_ip), ip) == 0) {
        counter_endpoint = connection->request_endpoint;
    }

    if (counter_endpoint == NULL) {
        ERR("counter endpoint not found\n");
        return -1;
    }

    if (get_stream_port(counter_endpoint->streams, type, port) == -1) {
        ERR("Cannot find counter part stream with type '%s'\n", type);
        return -1;
    }

    return 0;
}

int find_counter_endpoint(const char *ip, short unsigned int port, endpoint_t **endpoint, connection_t **connection_list) {
    *endpoint = NULL;

    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    while (current != NULL) {
        /*
         * Connection must have both request and response endpoints
         */
        if (has_request_and_response_endpoints(current) == 0) {
            // todo here we should check also for port not just IP (traverse all ports)
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

int remove_connection(char *call_id, connection_t **connection_list) {
    connection_t *prev;
    connection_t *next;
    connection_t *connection = NULL;

    if (find_connection_by_call_id(call_id, &connection, connection_list) == 0) {
        prev = connection->prev;
        next = connection->next;

        if (prev == NULL && next == NULL) {
            *connection_list = NULL;
        } else if (prev == NULL) {
            *connection_list = next;
            next->prev = NULL;
        } else if (next == NULL) {
            prev->next = NULL;
        } else {
            prev->next = next;
            next->prev = prev;
        }

        destroy_connection(connection);

        return 0;
    }

    INFO("Connection with '%s' call id was not found.\n", call_id);
    return -1;
}
