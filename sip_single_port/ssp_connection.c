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
    connection->same_ip = 0;
    connection->req_ip_alias = NULL;
    connection->res_ip_alias = NULL;

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

char *print_endpoint_aliases(alias_t *alias) {
    char *result = NULL;
    int success;

    alias_t *current;
    current = alias;

    while (current != NULL) {
        if (result == NULL) {
            success = asprintf(
                    &result,
                    " | %-39s |",
                    current->ip_port
            );
        } else {
            success = asprintf(
                    &result,
                    "%s\n | %-39s |",
                    result, current->ip_port
            );
        }

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }
        current = current->next;
    }

    if (result == NULL) {
        success = asprintf(
                &result,
                " | %-39s |",
                "not initialized - IPs differ"
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }
    }


    return result;
}

char *print_connection(connection_t *connection) {
    char *result;
    char *connection_info;
    char *request_endpoint_info;
    char *response_endpoint_info;
    int success;

    success = asprintf(
            &connection_info,
            "%s\n | %-39s |\n%s\n | %-39.*s |\n%s\n | %-18s | %-18s |\n%s\n | %-18s | %-18s |\n%s\n | %-39s |\n%s\n%s\n%s\n | %-39s |\n%s\n%s\n%s\n",
            get_hdr_line(0),
            "Connection by Call-ID",
            get_line(0),
            connection->call_id->len, connection->call_id->s,
            get_hdr_line(1),
            "Request IP",
            "Response IP",
            get_line(1),
            connection->request_endpoint_ip != NULL ? connection->request_endpoint_ip : "none",
            connection->response_endpoint_ip != NULL ? connection->response_endpoint_ip : "none",
            get_line(0),
            "Request aliases",
            get_line(0),
            print_endpoint_aliases(connection->req_ip_alias),
            get_line(0),
            "Response aliases",
            get_line(0),
            print_endpoint_aliases(connection->res_ip_alias),
            get_line(1)
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

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_connections_list() {

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return "not initialized yet\n";
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

int find_endpoint_by_alias(alias_t *aliases, char *ip_port) {
    alias_t *current_alias;
    current_alias = aliases;

    while (current_alias != NULL) {
        if (strcmp(current_alias->ip_port, ip_port) == 0) {
            return 1;
        }

        current_alias = current_alias->next;
    }

    return -1;
}

int get_counter_port(const char *ip, str type, connection_t *connection, unsigned short *port) {
    port = NULL;
    endpoint_t *counter_endpoint = NULL;

    if (connection->request_endpoint_ip == ip) {
        counter_endpoint = connection->response_endpoint;
    }

    if (connection->response_endpoint_ip == ip) {
        counter_endpoint = connection->request_endpoint;
    }

    if (counter_endpoint == NULL) {
        ERR("counter endpoint not found\n");
        return -1;
    }

    if (get_stream_port(counter_endpoint->streams, type, port) == -1) {
        ERR("Cannot find counter part stream with type '%.*s'\n", type.len, type.s);
        return -1;
    }

    return 0;
}

int find_counter_endpoint(const char *ip, short unsigned int port, endpoint_t **endpoint) {
    *endpoint = NULL;
    char *ip_port;
    int success;

    if (connections == NULL) {
        ERR("connections list is not initialized yet\n");
        return -1;
    }

    connection_t *current;
    current = connections;

    success = asprintf(
            &ip_port,
            "%s:%hu",
            ip, port
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return -1;
    }

    while (current != NULL) {
        /*
         * Connection must have both request and response endpoints
         */
        if (has_request_and_response_endpoints(current) == 0) {
            if (current->same_ip == 0) {
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
            } else {
                if (find_endpoint_by_alias(current->req_ip_alias, ip_port) == 1) {
                    *endpoint = current->response_endpoint;
                    if (current->response_endpoint->sibling == NULL) {
                        current->response_endpoint->sibling = current->request_endpoint;
                    }

                    return 0;
                }

                if (find_endpoint_by_alias(current->res_ip_alias, ip_port) == 1) {
                    *endpoint = current->request_endpoint;
                    if (current->request_endpoint->sibling == NULL) {
                        current->request_endpoint->sibling = current->response_endpoint;
                    }

                    return 0;
                }
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

int fill_in_aliases(connection_t *connection) {
    add_aliases(connection->request_endpoint_ip, connection->request_endpoint, &(connection->req_ip_alias));
    add_aliases(connection->response_endpoint_ip, connection->response_endpoint, &(connection->res_ip_alias));

    return 0;
}

int add_aliases(char *ip, endpoint_t *endpoint, alias_t **aliases) {
    char *rtcp;
    int success;
    endpoint_stream_t *current;
    alias_t *head = NULL;
    alias_t *current_alias = NULL;

    current = endpoint->streams;

    while (current != NULL) {
        alias_t *tmp = NULL;

        if (strcmp(current->port_raw, "0") == 0) {
            INFO("skipping stream with not defined ports eg. RTP == 0\n");
            current = current->next;
            continue;
        }

        tmp = create_alias(ip, current->port_raw);

        if (tmp == NULL) {
            ERR("cannot create alias");
            return -1;
        }

        if (head == NULL) {
            head = tmp;
            current_alias = head;
        } else {
            current_alias->next = tmp;
            current_alias = current_alias->next;
        }

        if (current->rtcp_port->len == 0) {
            success = asprintf(
                    &rtcp,
                    "%d",
                    atoi(current->port_raw) + 1
            );

            if (success == -1) {
                ERR("asprintf failed to allocate memory\n");
                return -1;
            }
        } else {
            rtcp = current->rtcp_port_raw;
        }

        tmp = create_alias(ip, rtcp);

        if (tmp == NULL) {
            ERR("cannot create alias");
            return -1;
        }

        if (head == NULL) {
            head = tmp;
            current_alias = head;
        } else {
            current_alias->next = tmp;
            current_alias = current_alias->next;
        }

        current = current->next;
    }

    *aliases = head;

    return 0;
}

alias_t *create_alias(char *ip, char *port) {
    int success;
    alias_t *tmp_alias;

    tmp_alias = pkg_malloc(sizeof(alias_t));
    if (tmp_alias == NULL) {
        ERR("cannot allocate pkg memory");
        return NULL;
    }

    tmp_alias->next = NULL;

    success = asprintf(
            &(tmp_alias->ip_port),
            "%s:%s",
            ip, port
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return tmp_alias;
}
