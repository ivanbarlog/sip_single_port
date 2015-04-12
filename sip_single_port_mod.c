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

#include "ssp_parse.h"
#include "ssp_replace.h"
#include "ssp_functions.h"
#include "ssp_endpoint.h"
#include "ssp_connection.h"
#include "ssp_stream.h"
#include "ssp_media_forward.h"


MODULE_VERSION

/** module functions */
static int mod_init(void);

int msg_received(void *data);

int msg_sent(void *data);

/** module parameters */
str _host_uri = {0, 0};
str _host_port = str_init("5060");

static param_export_t params[] = {
        {"host_uri",  PARAM_STR, &_host_uri},
        {"host_port", PARAM_STR, &_host_port},
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

#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
	#endif

    return 0;
}

/**
 *
 */
int msg_received(void *data) {
    void **d = (void **) data;
    void *d1 = d[0];
    void *d2 = d[1];

    char *s = (char *) d1;
    int len = *(unsigned int *) d2;

    sip_msg_t msg;
    str *obuf;

    obuf = (str *) pkg_malloc(sizeof(str));

    obuf->s = s;
    obuf->len = len;


    memset(&msg, 0, sizeof(sip_msg_t));
    msg.buf = obuf->s;
    msg.len = obuf->len;

    str *call_id = NULL;
    int msg_type = get_msg_type(&msg);

    switch (msg_type) {
        case SSP_SIP_REQUEST: //no break
        case SSP_SIP_RESPONSE:
            if (parse_call_id(&msg, call_id) == -1) {
                ERR("Cannot parse Call-ID\n");
                goto done;
            }

            if (initializes_dialog(&msg) == 0) {
                endpoint_t *endpoint = NULL;
                if (parse_endpoint(&msg, endpoint) == -1) {
                    ERR("Cannot parse Endpoint\n");
                    goto done;
                }

                connection_t *connection = NULL;
                if (find_connection_by_call_id(*call_id, connection) == -1) {
                    if (create_connection(*call_id, connection) == -1) {
                        ERR("Cannot create connection.\n");
                        goto done;
                    }

                    if (push_connection(connection) == -1) {
                        ERR("Cannot push connection to list\n");
                        goto done;
                    }
                }

                if (connection->request_endpoint == NULL && msg_type == SSP_SIP_REQUEST) {
                    connection->request_endpoint = endpoint;
                    connection->request_endpoint_ip = endpoint->ip;
                }

                if (connection->response_endpoint == NULL && msg_type == SSP_SIP_RESPONSE) {
                    connection->response_endpoint = endpoint;
                    connection->response_endpoint_ip = endpoint->ip;
                }
            }

            if (terminates_dialog(&msg) == 0) {
                remove_connection(*call_id);
            }

            break;
        case SSP_RTP_PACKET: //no break
        case SSP_RTCP_PACKET:
            INFO("RTP/RTCP packet\n");

            struct receive_info *ri;
            char *src_ip;
            unsigned short src_port, dst_port;
            struct sockaddr_in *dst_ip = NULL;
            int success;

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

            endpoint_t *endpoint = NULL;
            if (find_counter_endpoint_by_ip(src_ip, endpoint) != 0) {
                ERR("Cannot find counter part endpoint\n");
                goto done;
            }

            str *type = NULL;
            get_stream_type(endpoint->streams, src_port, type);
            get_stream_port(endpoint->streams, *type, &dst_port);

            get_socket_addr(endpoint->ip, dst_port, dst_ip);

            send_packet_to_endpoint(obuf, *dst_ip);

            break;
        default:
            goto done;
            break;
    }

    done:
    free_sip_msg(&msg);

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

    if (change_media_ports(&msg, _host_port) == 0) {
        obuf->s = update_msg(&msg, (unsigned int *) &obuf->len);
    }

    done:
    free_sip_msg(&msg);

    return 0;
}
