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

    if (connection->call_id != NULL) {
        shm_free(connection->call_id);
    }

    if (connection->request_endpoint != NULL) {
        destroy_endpoint(connection->request_endpoint);
    }

    if (connection->response_endpoint != NULL) {
        destroy_endpoint(connection->response_endpoint);
    }

    // we don't need to free next, prev, request and response endpoint IP
    // since they are just pointers and will be freed after whole connection is

    shm_free(connection);
}

connection_t *create_connection(char *call_id) {
    connection_t *connection = (connection_t *) shm_malloc(sizeof(connection_t));

    if (connection == NULL) {
        ERR("cannot allocate shm memory\n");
        return NULL;
    }

    connection->next = NULL;
    connection->prev = NULL;

    connection->call_id = NULL;

    connection->request_endpoint = NULL;
    connection->response_endpoint = NULL;

    connection->request_endpoint_ip = NULL;
    connection->response_endpoint_ip = NULL;

    shm_copy_string(call_id, (int) strlen(call_id), &(connection->call_id));

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

static int find_sibling_endpoint_by_ip_and_port(
        endpoint_t *endpoint1,
        endpoint_t *endpoint2,
        endpoint_t **endpoint,
        const char *ip,
        short unsigned int port
) {

    DBG("COMPARE %s & %s\n", endpoint1->ip, ip);
    if (strcmp(endpoint1->ip, ip) == 0) {

        DBG("IP match.\n");

        // matching IP, find out if port will match
        if (contain_port(endpoint1->streams, &port) == 0) {
            *endpoint = endpoint2;

            DBG("IP:port match.\n");

            if (endpoint2->sibling == NULL) {
                endpoint2->sibling = endpoint1;
            }

            return 0;
        }
    }

    DBG("IP:port not matching\n");

    return -1;
}

static int match_endpoint(
        endpoint_t *endpoint,
        const char *ip,
        short unsigned int port
) {
    if (strcmp(endpoint->ip, ip) != 0) {
        INFO("IP not matching.\n");

        return -1;
    }

    if (contain_port(endpoint->streams, &port) != 0) {
        INFO("port not matching.\n");

        return -1;
    }

    DBG("IP:port matching\n");

    return 0;
}

static int add_temporary_streams(endpoint_t *endpoint, char *port) {
    endpoint_stream_t *current;
    current = endpoint->streams;

    endpoint_stream_t *temporary = NULL;
    endpoint_stream_t *next = NULL;

    while (current != NULL) {
        next = current->next;

        temporary = (endpoint_stream_t *) shm_malloc(sizeof(endpoint_stream_t));
        temporary->temporary = 1;

        if (shm_copy_string(current->media, (int) strlen(current->media), &(temporary->media)) == -1) {
            ERR("cannot copy string.\n");
            destroy_stream(temporary);

            return -1;
        }

        if (shm_copy_string(port, (int) strlen(port), &(temporary->port)) == -1) {
            ERR("cannot copy string.\n");
            destroy_stream(temporary);

            return -1;
        }

        if (shm_copy_string(port, (int) strlen(port), &(temporary->rtcp_port)) == -1) {
            ERR("cannot copy string.\n");
            destroy_stream(temporary);

            return -1;
        }

        current->next = temporary;
        temporary->next = next;

        current = next;
        temporary = NULL;
    }

    return 0;
}

static int remove_temporary_streams(endpoint_t *endpoint) {
    endpoint_stream_t *current = endpoint->streams;
    endpoint_stream_t *temporary = NULL;
    endpoint_stream_t *prev = NULL;

    int ctr = -1;

    if (current == NULL) {
        ERR("endpoint has no streams.\n");

        return ctr;
    }

    do {
        // switch temporary and old streams
        current->temporary = !current->temporary;

        if (current->temporary == 1) {
            temporary = current;

            /*
             * when we are about to remove head of streams
             * we need to make sure that we link streams back to endpoint
             */
            if (endpoint->streams == current) {
                endpoint->streams = current->next;
            }

            current = current->next;

            if (prev != NULL) {
                prev->next = current;
            }

            // free temporary stream
            shm_free(temporary);
            ctr++;
        }

        if (current != NULL) {
            prev = current;
            current = current->next;
        }

    } while (current != NULL);

    return ctr;
}

int add_new_in_rule(
        const char *ip,
        short unsigned int port,
        char *new_port,
        connection_t **connection_list
) {
    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    int ctr = -1;

    while (current != NULL) {

        if (current->request_endpoint && match_endpoint(current->request_endpoint, ip, port) == 0) {
            add_temporary_streams(current->request_endpoint, new_port);
            ctr++;
        }

        if (current->response_endpoint && match_endpoint(current->response_endpoint, ip, port) == 0) {
            add_temporary_streams(current->response_endpoint, new_port);
            ctr++;
        }

        current = current->next;
    }

    return ctr;
}

int remove_temporary_rules(
        const char *ip,
        short unsigned int port,
        connection_t **connection_list
) {
    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    int ctr = -1;

    while (current != NULL) {

        if (current->request_endpoint && match_endpoint(current->request_endpoint, ip, port) == 0) {
            remove_temporary_streams(current->request_endpoint);
            ctr++;
        }

        if (current->response_endpoint && match_endpoint(current->response_endpoint, ip, port) == 0) {
            remove_temporary_streams(current->response_endpoint);
            ctr++;
        }

        current = current->next;
    }

    return ctr;
}

int change_socket_for_endpoints(
        const char *ip,
        short unsigned int port,
        struct socket_info *new_socket,
        connection_t **connection_list
) {
    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    int ctr = -1;

    while (current != NULL) {

        if (current->request_endpoint && match_endpoint(current->request_endpoint, ip, port) == 0) {
            current->request_endpoint->socket = new_socket;
            ctr++;
        }

        if (current->response_endpoint && match_endpoint(current->response_endpoint, ip, port) == 0) {
            current->response_endpoint->socket = new_socket;
            ctr++;
        }

        current = current->next;
    }

    return ctr;
}

int find_counter_endpoint(
        const char *ip,
        short unsigned int port,
        endpoint_t **endpoint,
      connection_t **connection_list
) {
    *endpoint = NULL;

    if (*connection_list == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = *connection_list;

    while (current != NULL) {

        // Connection must have both request and response endpoints
        if (has_request_and_response_endpoints(current) == 0) {

            DBG("Response endpoint streams\n");
            if (find_sibling_endpoint_by_ip_and_port(
                    current->request_endpoint,
                    current->response_endpoint,
                    endpoint,
                    ip,
                    port) == 0) {
                return 0;
            }

            DBG("Request endpoint streams\n");
            if (find_sibling_endpoint_by_ip_and_port(
                    current->response_endpoint,
                    current->request_endpoint,
                    endpoint,
                    ip,
                    port) == 0) {
                return 0;
            }
        }

        current = current->next;
    }

    INFO("IP '%s:%d' was not found in connections\n", ip, port);
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

void lock(connections_list_t *list) {
    lock_get(list->lock);
}

void unlock(connections_list_t *list) {
    lock_release(list->lock);
}
