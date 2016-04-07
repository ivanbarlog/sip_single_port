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


MODULE_VERSION

/**
 * Head of connections list
 */
static connection_t *connections_list = NULL;

/** module functions */
static int mod_init(void);

int msg_received(void *data);

int msg_sent(void *data);

typedef enum proxy_mode {
    SINGLE_PROXY_MODE,
    DUAL_PROXY_MODE,
} proxy_mode;

static int mode = SINGLE_PROXY_MODE;

static param_export_t params[] = {
        {"mode", INT_PARAM, &mode},
        {0, 0,                   0}
};

/** module exports */
struct module_exports exports = {
        "sip_single_port",
        DEFAULT_DLFLAGS, /* dlopen flags */
        0,
        params, /* params */
        0,          /* exported statistics */
        0,          /* exported MI functions */
        0,          /* exported pseudo-variables */
        0,          /* extra processes */
        mod_init,   /* module initialization function */
        0,
        0,
        0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void) {

    sr_event_register_cb(SREV_NET_DGRAM_IN, msg_received);
    sr_event_register_cb(SREV_NET_DATA_OUT, msg_sent);

    // allocate connections list
    connections_list = (struct connection *) shm_malloc(sizeof(struct connection));

    #ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
	#endif

    return 0;
}

const char delim[2] = ":";

int msg_received(void *data) {
    void **d = (void **) data;
    void *d1 = d[0];
    void *d2 = d[1];

    char *s = (char *) d1;
    int len = *(unsigned int *) d2;

    sip_msg_t msg;
    str *obuf;

    obuf = (str *) pkg_malloc(sizeof(str));
    if (obuf == NULL) {
        ERR("cannot allocate pkg memory");
        goto done;
    }

    obuf->s = s;
    obuf->len = len;

    if (obuf->len == 0 || strlen(obuf->s) == 0) {
        ERR("skipping empty packet");
        return 0;
    }

    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    str call_id;
    int msg_type = get_msg_type(&msg);

    int success;
    char *src_ip = NULL;
    char *tag = NULL;
    str* type = NULL;
    char *original_msg = NULL;
    char *modified_msg = NULL;
    char *call_id_c = NULL, *media_type_c = NULL;
    str *call_id_str = NULL;

    switch (msg_type) {
        case SSP_SIP_REQUEST: //no break
        case SSP_SIP_RESPONSE:
            if (parse_call_id(&msg, &call_id) == -1) {
                ERR("Cannot parse Call-ID\n");
                goto done;
            }

            if (initializes_dialog(&msg) == 0) {
                endpoint_t *endpoint;
                endpoint = parse_endpoint(&msg);
                if (endpoint == NULL) {
                    ERR("Cannot parse Endpoint\n");
                    goto done;
                }

                connection_t *connection = NULL;
                if (find_connection_by_call_id(call_id, &connection, &connections_list) == -1) {

                    connection = create_connection(call_id);
                    if (connection == NULL) {
                        ERR("Cannot create connection.\n");
                        goto done;
                    }

                    int connections_count = push_connection(connection, &connections_list);
                    DBG("%d. connection pushed to connections list\n", connections_count);
                }

                if (connection->request_endpoint == NULL && msg_type == SSP_SIP_REQUEST) {
                    connection->request_endpoint = pkg_malloc(sizeof(endpoint_t));
                    if (connection->request_endpoint == NULL) {
                        ERR("cannot allocate pkg memory");
                        goto done;
                    }

                    connection->request_endpoint = endpoint;
                    connection->request_endpoint_ip = &(endpoint->ip);
                    endpoint->call_id = connection->call_id;
                }

                if (connection->response_endpoint == NULL && msg_type == SSP_SIP_RESPONSE) {
                    connection->response_endpoint = pkg_malloc(sizeof(endpoint_t));
                    if (connection->response_endpoint == NULL) {
                        ERR("cannot allocate pkg memory");
                        goto done;
                    }

                    connection->response_endpoint = endpoint;
                    connection->response_endpoint_ip = &(endpoint->ip);
                    endpoint->call_id = connection->call_id;
                }
            }

            if (cancells_dialog(&msg) == 0) {
                remove_connection(call_id, &connections_list);
            }

            if (terminates_dialog(&msg) == 0) {
                remove_connection(call_id, &connections_list);
            }

            LM_DBG("\n\n CONNECTIONS LIST:\n\n%s\n\n", print_connections_list(&connections_list));

            break;
        case SSP_RTP_PACKET: //no break
        case SSP_RTCP_PACKET:
            INFO("RTP/RTCP packet\n");

            struct receive_info *ri;
            unsigned short src_port, dst_port;

            ri = (struct receive_info *) d[2];
            src_port = ri->src_port;
            success = asprintf(
                    &src_ip,
                    "%d.%d.%d.%d",
                    ri->src_ip.u.addr[0],
                    ri->src_ip.u.addr[1],
                    ri->src_ip.u.addr[2],
                    ri->src_ip.u.addr[3]
            );

            if (success == -1) {
                ERR("asprintf failed to allocate memory\n");
                goto done;
            }

            endpoint_t *dst_endpoint = NULL;
            if (find_counter_endpoint(src_ip, src_port, &dst_endpoint, &connections_list) != 0) {
                ERR("Cannot find counter part endpoint\n");
                goto done;
            }
            endpoint_t *src_endpoint = dst_endpoint->sibling;

            str *type = NULL;

            if (mode == SINGLE_PROXY_MODE) {
                if (msg_type == SSP_RTP_PACKET) {
                    if (get_stream_type(src_endpoint->streams, src_port, &type) == -1) {
                        ERR("Cannot find stream with port '%d'\n", src_port);
                        goto done;
                    }

                    if (get_stream_port(dst_endpoint->streams, *type, &dst_port) == -1) {
                        ERR("Cannot find counter part stream with type '%.*s'\n", type->len, type->s);
                        goto done;
                    }
                } else {
                    if (get_stream_type_rtcp(src_endpoint->streams, src_port, &type) == -1) {
                        ERR("Cannot find stream with port '%d'\n", src_port);
                        goto done;
                    }

                    if (get_stream_rtcp_port(dst_endpoint->streams, *type, &dst_port) == -1) {
                        ERR("Cannot find counter part stream with type '%.*s'\n", type->len, type->s);
                        goto done;
                    }
                }
            } else if (mode == DUAL_PROXY_MODE) {
                int tag_length;
                unsigned char first_byte = obuf->s[0];

                INFO("\n\n>>> Received obuf:\n\n%s\n\n", print_hex_str(obuf));

                // the RTP/RTCP packet is already modified
                if ((first_byte & BIT7) != 0 && (first_byte & BIT6) != 0) {
                    INFO("DUAL_PROXY_MODE changed RTP\n");

                    success = asprintf(
                            &tag,
                            "%s",
                            &(obuf->s[1])
                    );

                    if (success == -1) {
                        ERR("asprintf failed to allocate memory\n");
                        goto done;
                    }

                    tag_length = strlen(tag) + 1;

                    call_id_c = strtok(tag, delim);
                    media_type_c = strtok(NULL, delim);

                    INFO("\n\n\n>>> call_id: %s, media_type: %s\nhex: %s\n\n\n", call_id_c, media_type_c, print_hex(media_type_c));

                    call_id_str = (str *) pkg_malloc(sizeof(str));
                    call_id_str->s = call_id_c;
                    call_id_str->len = strlen(call_id_c);

                    type = (str *) pkg_malloc(sizeof(str));
                    type->s = media_type_c;
                    type->len = strlen(media_type_c);

                    connection_t *connection = NULL;
                    if (find_connection_by_call_id(*call_id_str, &connection, &connections_list) == -1) {
                        ERR("cannot find connection\n");
                        goto done;
                    }

                    if (get_counter_port(src_ip, *type, connection, &dst_port) == -1) {
                        ERR("cannot find destination port\n");
                        goto done;
                    }

                    int original_msg_length = sizeof(char) * (obuf->len - tag_length - 1);

                    // create final_msg which has length of original message - 2B for tag_length - tag_length B
                    original_msg = pkg_malloc(original_msg_length);
                    memcpy(original_msg, &(obuf->s[1 + tag_length]), original_msg_length);

                    obuf->s = original_msg;
                    obuf->len = original_msg_length;

                    INFO("\n\n>>> Original obuf:\n\n%s\n\n", print_hex_str(obuf));

                } else {
                    INFO("DUAL_PROXY_MODE original RTP\n(changing RPT)\n");

                    if (msg_type == SSP_RTP_PACKET) {
                        if (get_stream_type(src_endpoint->streams, src_port, &type) == -1) {
                            ERR("Cannot find stream with port '%d'\n", src_port);
                            goto done;
                        }

                        if (get_stream_port(dst_endpoint->streams, *type, &dst_port) == -1) {
                            ERR("Cannot find counter part stream with type '%.*s'\n", type->len, type->s);
                            goto done;
                        }
                    } else {
                        if (get_stream_type_rtcp(src_endpoint->streams, src_port, &type) == -1) {
                            ERR("Cannot find stream with port '%d'\n", src_port);
                            goto done;
                        }

                        if (get_stream_rtcp_port(dst_endpoint->streams, *type, &dst_port) == -1) {
                            ERR("Cannot find counter part stream with type '%.*s'\n", type->len, type->s);
                            goto done;
                        }
                    }

                    char tag_s[src_endpoint->call_id->len + type->len + 1];
                    sprintf(
                            tag_s,
                            "%.*s:%.*s",
                            src_endpoint->call_id->len, src_endpoint->call_id->s,
                            type->len, type->s
                    );
                    tag_s[src_endpoint->call_id->len + type->len + 1] = '\0';

                    // add byte for '\0' to length
                    tag_length = sizeof(char) * strlen(tag_s);
                    int original_length = obuf->len;

                    INFO("DUAL_PROXY_MODE tag (%d): '%s'\n", tag_length, tag_s);

                    // 1B is for modified RTP/RTCP v3 header
                    int modified_msg_length = 1 + tag_length + 1 + original_length;
                    modified_msg = pkg_malloc(sizeof(char) * modified_msg_length);

                    // set first byte to `bin(11xxxxxx)`
                    unsigned char changed_first_byte = first_byte | 0xc0;
                    memcpy(modified_msg, &changed_first_byte, sizeof(unsigned char));

                    memcpy(&(modified_msg[1]), &tag_s, tag_length + 1);

                    // copy rest of the original message after the tag
                    memcpy(&(modified_msg[2 + tag_length]), obuf->s, sizeof(char) * original_length);

                    obuf->s = modified_msg;
                    obuf->len = modified_msg_length;

                    INFO("\n\n>>> Modified obuf:\n\n%s\n\n", print_hex_str(obuf));
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

            if (send_packet_to_endpoint(obuf, *dst_ip) == 0) {
                INFO("RTP packet sent successfully!\n");
            }

            break;
        default:
            goto done;
    }

    done:
    free_sip_msg(&msg);
    if (tag != NULL) {
        // we need to use free instead of pkg_free since asprintf uses malloc
        free(tag);
    }
    if (src_ip != NULL) {
        // we need to use free instead of pkg_free since asprintf uses malloc
        free(src_ip);
    }
//    if (call_id_c != NULL) {
//        // we need to use free instead of pkg_free since strtok uses malloc
//        free(call_id_c);
//    }
//    if (media_type_c != NULL) {
//        // we need to use free instead of pkg_free since strtok uses malloc
//        free(media_type_c);
//    }
    if (modified_msg != NULL) {
        pkg_free(modified_msg);
    }
    if (original_msg != NULL) {
        pkg_free(original_msg);
    }
    if (obuf != NULL) {
        pkg_free(obuf);
    }
    if (call_id_str != NULL) {
        pkg_free(call_id_str);
    }
    if (type != NULL) {
        pkg_free(type);
    }

    return 0;
}

int msg_sent(void *data) {
    sip_msg_t msg;
    str *obuf;

    obuf = (str *) data;
    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    if (skip_media_changes(&msg) == -1) {
        goto done;
    }

    if (change_media_ports(&msg, bind_address) == -1) {
        goto done;
    }

    obuf->s = update_msg(&msg, (unsigned int *) &obuf->len);

    done:
    free_sip_msg(&msg);

    return 0;
}
