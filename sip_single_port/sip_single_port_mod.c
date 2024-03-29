/**
 * $Id$
 *
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Extensible SIP Router, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief SIP-router sip_single_port :: Module interface
 * \ingroup sip_single_port
 * Module: \ref sip_single_port
 */

/*! \defgroup sip_single_port SIP-router :: SIP Single Port
 *
 * This module was created as a diploma thesis at Slovak University
 * of Technology in Bratislava Faculty of Informatics and Information
 * Technology. Module is dedicated to prove concept of SIP Single Port
 * architecture in which not only SIP traffic is routed via SIP server
 * eg. Kamailio but also all data streams (RTP and RTCP) so the stream
 * is more manageable since it is flowing over same TCP port.
 */

#define _GNU_SOURCE //allows us to use asprintf

/*
 * uncomment for debugging purposes
 *
 * CONNECTIONS LIST will be shown but it contains memory leaks
 * so it's not suitable for production
 */
//#define DEBUG_BUILD 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../forward.h"
#include "../../globals.h"

#include "../../mem/shm_mem.h"

#include "ssp_parse.h"
#include "ssp_replace.h"
#include "ssp_functions.h"
#include "ssp_endpoint.h"
#include "ssp_connection.h"
#include "ssp_stream.h"
#include "ssp_media_forward.h"
#include "ssp_bind_address.h"
#include "ssp_rtcp.h"
#include "ssp_bind_address.h"


MODULE_VERSION

/**
 * Head of connections list
 */
connections_list_t *connections_list = NULL;

#define _connections_list (connections_list)

socket_list_t *socket_list = NULL;

#define _socket_list (socket_list)

/** module functions */
static int mod_init(void);

static int child_init(int);

static void mod_destroy(void);

int msg_received(void *data);

int msg_sent(void *data);

typedef enum proxy_mode {
    SINGLE_PROXY_MODE,
    DUAL_PROXY_MODE,
} proxy_mode;

typedef enum instance_mode {
    CLIENT_INSTANCE,
    SERVER_INSTANCE,
} instance_mode;

static int mode = SINGLE_PROXY_MODE;
static int instance = CLIENT_INSTANCE;
static int packet_loss_threshold = 0;

static param_export_t params[] = {
        {"mode", INT_PARAM, &mode},
        {"instance", INT_PARAM, &instance},
        {"packet_loss_threshold", INT_PARAM, &packet_loss_threshold},
        {0, 0, 0}
};

/**
 * Adds rule for incoming packets
 * eg. functions finds endpoint by provided IP:port
 * and adds temporary streams with provided new port
 */
static int m_add_in_rule(sip_msg_t *msg, char *f_ip, char *f_port, char *t_port);
/**
 * Finds endpoint by provided IP:port and replace old rules with temporary one
 */
static int m_remove_in_rule(sip_msg_t *msg, char *s_ip, char *s_port);
/**
 * Finds endpoint by provided IP:port (s_ip:s_port) and replace it's sending socket with socket
 * matching provided IP:port (t_ip:t_port)
 */
static int m_change_socket(sip_msg_t *msg, char *s_ip, char *s_port, char *t_ip, char *t_port);

static int m_subscribe_client(sip_msg_t *msg, char *ip, char *port);

static cmd_export_t cmds[] = {
        {"add_in_rule", (cmd_function) m_add_in_rule, 3, NULL, 0, ANY_ROUTE},
        {"remove_in_rule", (cmd_function) m_remove_in_rule, 2, NULL, 0, ANY_ROUTE},
        {"change_socket", (cmd_function) m_change_socket, 4, NULL, 0, ANY_ROUTE},
        {"subscribe_client", (cmd_function) m_subscribe_client, 2, NULL, 0, ANY_ROUTE},
        {0, 0, 0, 0, 0, 0}
};

/** module exports */
struct module_exports exports = {
        "sip_single_port",
        DEFAULT_DLFLAGS, /* dlopen flags */
        cmds,
        params, /* params */
        0,          /* exported statistics */
        0,          /* exported MI functions */
        0,          /* exported pseudo-variables */
        0,          /* extra processes */
        mod_init,   /* module initialization function */
        0,
        (destroy_function) mod_destroy,
        child_init           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void) {

    sr_event_register_cb(SREV_NET_DGRAM_IN, msg_received);
    sr_event_register_cb(SREV_NET_DATA_OUT, msg_sent);

    // initialize connection list
    connections_list = (connections_list_t *) shm_malloc(sizeof(connections_list_t));

    if (connections_list == NULL) {
        ERR("cannot allocate connections list\n");
        return -1;
    }

    socket_list = (socket_list_t *) shm_malloc(sizeof(socket_list_t));
    if (socket_list == NULL) {
        ERR("cannot allocate socket list\n");
        return -1;
    }

    socket_list->lock = lock_alloc();
    if (socket_list->lock == NULL) {
        ERR("cannot allocate the lock for the socket list\n");
        return -1;
    }

    if (lock_init(socket_list->lock) == NULL) {
        ERR("lock initialization failed\n");
        return -1;
    }

#ifdef USE_TCP
    tcp_set_clone_rcvbuf(1);
#endif

    return 0;
}

static int child_init(int rank) {
    if (rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
        return 0; /* do nothing for the main process */

    if (_connections_list == NULL) {
        ERR("connections list not initialized in main process\n");
        return -1;
    }

    connections_list = _connections_list;

    if (_socket_list == NULL) {
        ERR("socket list not initialized in main process\n");
        return -1;
    }

    socket_list = _socket_list;

    return 0;
}

static void mod_destroy(void) {
    if (connections_list) {
        connection_t *current_connection = connections_list->head;
        while (current_connection != NULL) {
            destroy_connection(current_connection);

            current_connection = current_connection->next;
        }

        shm_free(connections_list);
    }

    if (socket_list) {
        socket_item_t *current_item = socket_list->head;
        while (current_item != NULL) {
            shm_free(current_item);

            current_item = current_item->next;
        }

        shm_free(socket_list);
    }
}

int msg_received(void *data) {
    void **d = (void **) data;
    void *d1 = d[0];
    void *d2 = d[1];

    char *s = (char *) d1;
    int len = *(unsigned int *) d2;

    sip_msg_t msg;
    str *obuf;

    obuf = (str *) data;

    obuf->s = s;
    obuf->len = len;

    if (obuf->len == 0 || strlen(obuf->s) == 0) {
        ERR("skipping empty packet\n");
        return 0;
    }

    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = (unsigned int) obuf->len;

    str str_call_id;
    char *shm_call_id = NULL;
    char *pkg_call_id = NULL;
    char *pkg_media_type = NULL;

    int msg_type = get_msg_type(&msg);

    struct receive_info *ri = (struct receive_info *) d[2];

#ifdef DEBUG_BUILD
    print_socket_addresses(get_first_socket());
#endif

    switch (msg_type) {
        case SSP_SIP_REQUEST: //no break
        case SSP_SIP_RESPONSE:

            if (parse_call_id(&msg, &str_call_id) == -1) {
                ERR("Cannot parse Call-ID\n");
                goto done;
            }

            shm_copy_string(str_call_id.s, str_call_id.len, &shm_call_id);

            if (initializes_dialog(&msg) == 0) {
                endpoint_t *endpoint;
                endpoint = parse_endpoint(&msg);
                if (endpoint == NULL) {
                    ERR("Cannot parse Endpoint\n");
                    goto done;
                }

                connection_t *connection = NULL;
                if (find_connection_by_call_id(shm_call_id, &connection, &(connections_list->head)) == -1) {

                    // if connection was not found by Call-ID we'll create one
                    connection = create_connection(shm_call_id);
                    if (connection == NULL) {
                        ERR("Cannot create connection.\n");
                        goto done;
                    }

                    int connections_count = push_connection(connection, &(connections_list->head));
                    DBG("%d. connection pushed to connections list\n", connections_count);
                }

                if (connection->request_endpoint == NULL && msg_type == SSP_SIP_REQUEST) {

                    // add endpoint to connection
                    lock_connection(connection);
                    connection->request_endpoint = endpoint;
                    connection->request_endpoint_ip = &(endpoint->ip);
                    unlock_connection(connection);

                    // add Call-ID to endpoint for quicker searching
                    // set sending socket to default kamailio socket
                    lock_endpoint(endpoint);
                    endpoint->call_id = connection->call_id;
                    endpoint->socket = get_first_socket();
                    unlock_endpoint(endpoint);
                }

                if (connection->response_endpoint == NULL && msg_type == SSP_SIP_RESPONSE) {

                    // add endpoint to connection
                    lock_connection(connection);
                    connection->response_endpoint = endpoint;
                    connection->response_endpoint_ip = &(endpoint->ip);
                    unlock_connection(connection);

                    // add Call-ID to endpoint for quicker searching
                    // set sending socket to default kamailio socket
                    lock_endpoint(endpoint);
                    endpoint->call_id = connection->call_id;
                    endpoint->socket = get_first_socket();
                    unlock_endpoint(endpoint);
                }
            }

            if (cancels_dialog(&msg) == 0) {
                remove_connection(shm_call_id, &(connections_list->head));
            }

            if (terminates_dialog(&msg) == 0) {
                remove_connection(shm_call_id, &(connections_list->head));
            }

#ifdef DEBUG_BUILD
        char *cl_table = print_connections_list(&(connections_list->head));
            DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

            if (cl_table != NULL)
                free(cl_table);
#endif
            break;
        case SSP_RTP_PACKET: //no break
        case SSP_RTCP_PACKET:
            INFO("RTP/RTCP packet\n");

            char src_ip[16];
            unsigned short src_port, dst_port;
            char *media_type;

            if (connections_list->head == NULL) {
                ERR("connections list not initialized yet");
                goto done;
            }

            set_src_ip_and_port(src_ip, &src_port, ri);

#ifdef DEBUG_BUILD
            INFO(
                    "Received RTP/RTCP packet from %s:%d\n",
                    src_ip, src_port
            );
#endif

            endpoint_t *dst_endpoint = NULL;
            if (find_counter_endpoint(src_ip, src_port, &dst_endpoint, &(connections_list->head)) != 0) {
                ERR("Cannot find counter part endpoint\n");
                goto done;
            }
            endpoint_t *src_endpoint = dst_endpoint->sibling;

            if (mode == SINGLE_PROXY_MODE) {

                if (find_dst_port(msg_type, src_endpoint, dst_endpoint, src_port, &dst_port, &media_type) == -1) {
                    ERR("Cannot find destination port where the packet should be forwarded.\n");
                    goto done;
                }

            } else if (mode == DUAL_PROXY_MODE) {

                int tag_length;
                unsigned char first_byte = (unsigned char) obuf->s[0];

                // the RTP/RTCP packet is already modified
                if ((first_byte & BIT7) != 0 && (first_byte & BIT6) != 0) {
                    INFO("DUAL_PROXY_MODE changed RTP\n");

                    if (parse_tagged_msg(&(obuf->s[1]), &pkg_call_id, &pkg_media_type, &tag_length) == -1) {
                        ERR("Cannot parse tagged message.\n");
                        goto done;
                    }

                    connection_t *connection;
                    if (find_connection_by_call_id(pkg_call_id, &connection, &(connections_list->head)) == -1) {
                        ERR("cannot find connection\n");
                        goto done;
                    }

                    if (get_counter_port(src_ip, pkg_media_type, connection, &dst_port) == -1) {
                        ERR("cannot find destination port\n");
                        goto done;
                    }

                    if (remove_tag(obuf, tag_length) == -1) {
                        ERR("Cannot remove tag from tagged message.\n");
                        goto done;
                    }

                    unsigned char second_byte = (unsigned char) obuf->s[1];
                    int is_rtcp = (!!(second_byte & BIT6) == 1 && !!(second_byte & BIT5) == 0) ? 1 : 0;

                    // checking RTCP packet after tag was removed
                    if (instance == SERVER_INSTANCE && is_rtcp == 1 && exceeds_limit(obuf->s, packet_loss_threshold) == 1) {
                        // RTCP packet received from UA so we are sending notification to destination eg. kamailio client
                        notify(src_ip, src_port, _socket_list, ri->bind_address);
                    }

                } else { // RTP/RTCP packet is not modified yet so we are about to tag it

                    // checking RTCP packet before tagging it
                    if (instance == SERVER_INSTANCE && msg_type == SSP_RTCP_PACKET && exceeds_limit(obuf->s, packet_loss_threshold) == 1) {
                        // RTCP packet received from kamailio client so we are sending notification back to source
                        notify(dst_endpoint->ip, dst_port, _socket_list, ri->bind_address);
                    }

                    if (find_dst_port(msg_type, src_endpoint, dst_endpoint, src_port, &dst_port, &media_type) == -1) {
                        ERR("Cannot find destination port where the packet should be forwarded.\n");
                        goto done;
                    }

                    if (tag_message(obuf, src_endpoint->call_id, media_type) == -1) {
                        ERR("Cannot tag message.\n");
                        goto done;
                    }
                }

            } else {
                ERR("Unknown sip_single_port mode\n");
                goto done;
            }

            struct sockaddr_in *dst_ip = NULL;
            if (get_socket_addr(dst_endpoint->ip, dst_port, &dst_ip) == -1) {
                ERR("Cannot instantiate socket address\n");
                goto done;
            }

#ifdef DEBUG_BUILD
            INFO(
                    "Sending RTP/RTCP packet to %s:%d via %.*s:%.*s socket\n",
                    dst_endpoint->ip, dst_port,
                    dst_endpoint->socket->address_str.len, dst_endpoint->socket->address_str.s,
                    dst_endpoint->socket->port_no_str.len, dst_endpoint->socket->port_no_str.s
            );
#endif

            if (send_packet_to_endpoint(obuf, *dst_ip, dst_endpoint->socket) == 0) {
                INFO("RTP packet sent successfully!\n");
            }

            break;
        default:
            goto done;
    }

    done:

    free_sip_msg(&msg);

    if (shm_call_id != NULL)
        shm_free(shm_call_id);

    if (pkg_call_id != NULL)
        pkg_free(pkg_call_id);

    if (pkg_media_type != NULL)
        pkg_free(pkg_media_type);

    return 0;
}

int msg_sent(void *data) {
    sip_msg_t msg;
    str *obuf;

    obuf = (str *) data;

    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = (unsigned int) obuf->len;

    if (msg.buf == 0 || msg.len == 0) {
        ERR("empty message\n");
        goto done;
    }

    if (skip_media_changes(&msg) == -1) {
        DBG("Skipping SDP changes.\n");
        goto done;
    }

    // @todo find right socket by destination address msg.rcv->bind_address is empty
    if (change_media_ports(&msg, get_first_socket()) == -1) {
        ERR("Changing SDP failed.\n");
        goto done;
    }

    obuf->s = update_msg(&msg, (unsigned int *) &obuf->len);

    done:
    free_sip_msg(&msg);

    return 0;
}

static int m_add_in_rule(sip_msg_t *msg, char *f_ip, char *f_port, char *t_port) {

    if (f_ip == 0 || f_port == 0 || t_port == 0) {
        ERR("You must provide values for all parameters.\n");

        return -1;
    }

#ifdef DEBUG_BUILD
    INFO("ADD_IN_RULE()\n");
    char *cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    // find all endpoints matching IP:port and add temporary streams
    if (add_new_in_rule(f_ip, (unsigned short) atoi(f_port), t_port, &(connections_list->head)) == -1) {
        ERR("there are no endpoints where can be added the new rule.\n");

        return -1;
    }

#ifdef DEBUG_BUILD
    INFO("ADD_IN_RULE - rule ADDed\n");

    cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    return 1;
}

static int m_remove_in_rule(sip_msg_t *msg, char *s_ip, char *s_port) {

    if (s_ip == 0 || s_port == 0) {
        ERR("You must provide values for all parameters.\n");

        return -1;
    }

#ifdef DEBUG_BUILD
    INFO("REMOVE_IN_RULE()\n");
    char *cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    // find all endpoints matching IP:port and add temporary streams
    if (remove_temporary_rules(s_ip, (unsigned short) atoi(s_port), &(connections_list->head)) == -1) {
        ERR("there are no endpoints where can be removed temporary rules.\n");

        return -1;
    }

#ifdef DEBUG_BUILD
    INFO("REMOVE_IN_RULE - rule REMOVEd\n");
    cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    return 1;
}

static int m_change_socket(sip_msg_t *msg, char *s_ip, char *s_port, char *t_ip, char *t_port) {

#ifdef DEBUG_BUILD
    char *cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    str to_ip = {0, 0};
    str to_port = {0, 0};

    if (s_ip == 0 || s_port == 0 || t_ip == 0 || t_port == 0) {
        ERR("You must provide values for all parameters.\n");

        return -1;
    }

    to_ip.s = t_ip;
    to_ip.len = (int) strlen(t_ip);
    to_port.s = t_port;
    to_port.len = (int) strlen(t_port);

    struct socket_info *sockets = get_first_socket();
    struct socket_info *new_socket;

    new_socket = get_bind_address(to_ip, to_port, &sockets);

    if (new_socket == NULL) {
        ERR(
                "cannot find new socket by %.*s:%.*s\n",
                to_ip.len, to_ip.s,
                to_port.len, to_port.s
        );

        return -1;
    }

    if (change_socket_for_endpoints(s_ip, (unsigned short) atoi(s_port), new_socket, &(connections_list->head)) == -1) {
        ERR("zero sockets were changed.\n");

        return -1;
    }

    decrement_clients_count(msg->rcv.bind_address, _socket_list);
    increment_clients_count(new_socket, _socket_list);


#ifdef DEBUG_BUILD
    INFO("CHANGE_SOCKET - socket CHANGEd\n");
    cl_table = print_connections_list(&(connections_list->head));
    DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", cl_table == NULL ? "not initialized yet\n" : cl_table);

    if (cl_table != NULL)
        free(cl_table);
#endif

    return 1;
}

static int m_subscribe_client(sip_msg_t *msg, char *ip, char *port) {

    if (socket_list->head == NULL) {
        socket_item_t *head = init_socket_list(get_first_socket());
        lock_socket_list(socket_list);
        socket_list->head = head;
        unlock_socket_list(socket_list);

        if (socket_list->head == NULL) {
            ERR("cannot initialize socket list head.\n");
            shm_free(connections_list);
            shm_free(socket_list);
            return -1;
        }
    }

    str address = {0,0};
    str port_no = {0,0};
    address.s = ip;
    address.len = (int)strlen(ip);
    port_no.s = port;
    port_no.len = (int)strlen(port);

    struct socket_info *sockets = get_first_socket();
    struct socket_info *socket = get_bind_address(
            msg->rcv.bind_address->address_str,
            msg->rcv.bind_address->port_no_str,
            &sockets
    );

    INFO(
            "subscribe client: %.*s:%.*s (received on socket %.*s:%.*s)\n",
            address.len, address.s,
            port_no.len, port_no.s,
            msg->rcv.bind_address->address_str.len, msg->rcv.bind_address->address_str.s,
            msg->rcv.bind_address->port_no_str.len, msg->rcv.bind_address->port_no_str.s
    );

    print_socket_list(_socket_list);

    increment_clients_count(socket, _socket_list);

    print_socket_list(_socket_list);

    return 1;
}
