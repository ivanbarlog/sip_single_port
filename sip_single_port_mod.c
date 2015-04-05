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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../forward.h"

#include "ssp_funcs.h"
#include "ssp_endpoint.h"
#include "ssp_body.h"

MODULE_VERSION

/** module functions */
static int mod_init(void);
int msg_received(void *data);
int msg_sent(void *data);

/** module parameters */
str _host_uri = {0, 0};
str _host_port = str_init("5060");

static param_export_t params[]={
		{"host_uri",	PARAM_STR,	&_host_uri},
		{"host_port",	PARAM_STR,	&_host_port},
		{0,0,0}
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
//	sr_event_register_cb(SREV_NET_DATA_IN, tdb_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, msg_sent);

#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
	#endif

	return 0;
}

/**
 *
 */
int msg_received(void *data)
{
	void **d = (void **)data;
	void * d1 = d[0];
	void * d2 = d[1];

	char * s = (char *)d1;
	int len = *(int*)d2;

	sip_msg_t msg;
	str *obuf;

	obuf = (str *)pkg_malloc(sizeof(str));

	obuf->s = s;
	obuf->len = len;


	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	int msg_type = get_msg_type(&msg);

	endpoint_t *endpoint;
	connection_t *connection = NULL;

	char * call_id;

	switch (msg_type)
	{
		case SIP_REQ: // no break
			if (msg.callid == NULL) {
				LM_DBG("cannot parse message call id");
				goto done;
			}

			memcpy(&call_id, msg.callid->body.s, msg.callid->body.len);

			if (findConnection(call_id, connection) == -1) {
				connection = createConnection(call_id);
			}

			if (connection->request_endpoint != NULL) {
				endpoint = (endpoint_t *) pkg_malloc(sizeof(endpoint_t));

				if (parseEndpoint(&msg, endpoint, msg_type) == 0) {
					connection->request_endpoint = endpoint;
					printEndpoint(endpoint);
				}
			}

			break;

		case SIP_REP:
			if (msg.callid == NULL) {
				LM_DBG("cannot parse message call id");
				goto done;
			}

			memcpy(&call_id, msg.callid->body.s, msg.callid->body.len);

			if (findConnection(call_id, connection) == -1) {
				connection = createConnection(call_id);
			}

			if (connection->response_endpoint != NULL) {
				endpoint = (endpoint_t *) pkg_malloc(sizeof(endpoint_t));

				if (parseEndpoint(&msg, endpoint, msg_type) == 0) {
					connection->response_endpoint = endpoint;
					printEndpoint(endpoint);
				}
			}

//			endpoint = (endpoint_t *) pkg_malloc(sizeof(endpoint_t));
//
//			if (parseEndpoint(&msg, endpoint, msg_type) == 0) {
//				if (endpointExists(endpoint->ip, endpoint->type) == 1) {
//					LM_ERR("Endpoint already exists.");
//					goto done;
//				}
//
//				pushEndpoint(endpoint);
//
//				printEndpoint(endpoint);
//			}

			break;

		case RTP: //no break
		case RTCP:
			LM_DBG("Message type RTP/RTCP %d\n", msg_type);

			struct receive_info * ri;
			char src_ip[50];
			struct sockaddr_in dst_ip;

            ri = (struct receive_info*) d[2];

			sprintf(src_ip, "%d.%d.%d.%d",
					ri->src_ip.u.addr[0],
					ri->src_ip.u.addr[1],
					ri->src_ip.u.addr[2],
					ri->src_ip.u.addr[3]
			);

			LM_DBG(
					"AAA:\nsrc_ip: %s\nsrc_port: %d\ndst_port: %d\n",
					src_ip, ri->src_port, ri->dst_port
			);

			if (findConnectionBySrcIp(src_ip, connection) == 1) {
				LM_DBG("found connection %s\n", connection->call_id);
			}


//			endpoint_t *endpoint;
//
//			if (findEndpoint(head, ))
//
//			endpoint_stream_t *stream;
//
//			if (findStream(endpoint->streams, *stream, ri->dst_port) == 0)
//			{
//
//			}


//			if (ri != NULL && request_ep != NULL && reply_ep != NULL)
//			{
//
//
//				/**
//				 * compare src_ip with request and reply endpoint
//				 * to know where to send RTP packet
//				 */
//				LM_DBG("\n\nsrc_ip: %s\nrequest_ip: %s\nreply_ip: %s\n\n", src_ip, request_ep->ip, reply_ep->ip);
//
//				if (strcmp(src_ip, request_ep->ip))
//				{
//					LM_DBG("sending to request\n");
//					dst_ip = request_ep->ip_address;
//				}
//				else if (strcmp(src_ip, reply_ep->ip))
//				{
//					LM_DBG("sending to reply\n");
//					dst_ip = reply_ep->ip_address;
//				}
//				else
//				{
//					LM_DBG("do not know where to send packet\n");
//					goto done;
//				}
//
//				int sent_bytes = sendto(bind_address->socket, obuf->s, obuf->len, 0, (const struct sockaddr*) &dst_ip, sizeof(struct sockaddr_in));
//
//				if (sent_bytes != obuf->len)
//				{
//					LM_DBG("failed to send packet\n");
//					goto done;
//				}
//				else
//				{
//					LM_DBG("packet sent successfully\n");
//				}
//
//			}
//			else
//			{
//				LM_DBG("receive info is null\n");
//				goto done;
//			}

			break;
		default:
			LM_ERR("Unknown message type\n");
			goto done;
	}

done:
	free_sip_msg(&msg);

	return 0;
}

int msg_sent(void *data) {
	sip_msg_t msg;
	str *obuf;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if (skip_media_changes(&msg) < 0) {
		goto done;
	}

	if (changeRtpAndRtcpPort(&msg, _host_port, _host_uri) == 1) {
		obuf->s = update_msg(&msg, (unsigned int *) &obuf->len);
	}

done:
	free_sip_msg(&msg);

	return 0;
}
