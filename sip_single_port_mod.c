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

/*! \defgroup sip_single_port SIP-router :: Topology hiding
 *
 * This module hides the SIP routing headers that show topology details.
 * It it is not affected by the server being transaction stateless or
 * stateful. The script interpreter gets the SIP messages decoded, so all
 * existing functionality is preserved.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <bits/stdio2.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_methods.h"


#include "../../mem/mem.h"
#include "../../globals.h"

#include "../../re.h"

/*
MSG_BODY_SDP
application/sdp
*/
#include "../../parser/sdp/sdp.h"
/*
int extract_media_attr(str *body, str *mediamedia, str *mediaport, str *mediatransport, str *mediapayload, int *is_rtp);
int extract_rtcp(str *body, str *rtcp);
*/
#include "../../parser/sdp/sdp_helpr_funcs.h"
#include "../cdp_avp/avp_add.h"
#include "../cdp_avp/avp_new.h"
#include "../../msg_translator.h"
#include "../../data_lump.h"


#include "ssp_funcs.h"
#include "ssp_endpoint.h"

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
static int mod_init(void)
{
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

	switch (msg_type)
	{
		case SIP_REQ:
			if (request_ep == NULL)
			{
				LM_DBG("writing endpoint %d\n", msg_type);

				request_ep = (endpoint *)pkg_malloc(sizeof(endpoint));

				if (parseEndpoint(&msg, request_ep) == 0)
				{
					request_ep->type = msg_type;

					memset((char *) &(request_ep->ip_address), 0, sizeof(request_ep->ip_address));
					request_ep->ip_address.sin_family = AF_INET;
					request_ep->ip_address.sin_addr.s_addr = inet_addr(request_ep->ip);
					request_ep->ip_address.sin_port = htons(request_ep->rtp_port);

					printEndpoint(request_ep);
				}
				else
				{
					LM_DBG("reset request_ep\n");
					pkg_free(request_ep);
					request_ep = NULL;
				}
			}
			else
			{
				LM_DBG("not writing endpoint %d\n", msg_type);
			}
			break;

		case SIP_REP:
			if (reply_ep == NULL)
			{
				LM_DBG("writing endpoint %d\n", msg_type);

				reply_ep = (endpoint *)pkg_malloc(sizeof(endpoint));

				if (parseEndpoint(&msg, reply_ep) == 0)
				{
					reply_ep->type = msg_type;

				        memset((char *) &(reply_ep->ip_address), 0, sizeof(reply_ep->ip_address));
				        reply_ep->ip_address.sin_family = AF_INET;
				        reply_ep->ip_address.sin_addr.s_addr = inet_addr(reply_ep->ip);
		        		reply_ep->ip_address.sin_port = htons(reply_ep->rtp_port);

					printEndpoint(reply_ep);
				}
				else
				{
					LM_DBG("reset reply_ep\n");
					pkg_free(reply_ep);
					reply_ep = NULL;
				}
			}
			else
			{
				LM_DBG("not writing endpoint %d\n", msg_type);
			}
			break;

		case RTP: //no break
		case RTCP:
			LM_DBG("Message type RTP/RTCP %d\n", msg_type);

			struct receive_info * ri;
			char src_ip[50];
			struct sockaddr_in dst_ip;

            ri = (struct receive_info*) d[2];

			if (ri != NULL && request_ep != NULL && reply_ep != NULL)
			{
				/*int len =*/
				sprintf(src_ip, "%d.%d.%d.%d",
					ri->src_ip.u.addr[0],
					ri->src_ip.u.addr[1],
					ri->src_ip.u.addr[2],
					ri->src_ip.u.addr[3]
				);
				
				/**
				 * compare src_ip with request and reply endpoint
				 * to know where to send RTP packet
				 */
				LM_DBG("\n\nsrc_ip: %s\nrequest_ip: %s\nreply_ip: %s\n\n", src_ip, request_ep->ip, reply_ep->ip);

				if (strcmp(src_ip, request_ep->ip))
				{
					LM_DBG("sending to request\n");
					dst_ip = request_ep->ip_address;
				}
				else if (strcmp(src_ip, reply_ep->ip))
				{
					LM_DBG("sending to reply\n");
					dst_ip = reply_ep->ip_address;
				}
				else
				{
					LM_DBG("do not know where to send packet\n");
					goto done;
				}
				
				int sent_bytes = sendto(bind_address->socket, obuf->s, obuf->len, 0, (const struct sockaddr*) &dst_ip, sizeof(struct sockaddr_in));

				if (sent_bytes != obuf->len)
				{
					LM_DBG("failed to send packet\n");
					goto done;
				}
				else
				{
					LM_DBG("packet sent successfully\n");
				}

			}
			else
			{
				LM_DBG("receive info is null\n");
				goto done;
			}

			if (printed == 0)
			{
				printEndpoint(request_ep);
				printEndpoint(reply_ep);
				printed = 1;
			}

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


    // todo:
    // if INVITE changeRtpAndRtcpPort
    // if ~200 changeRtpAndRtcpPort

	if (skip_media_changes(&msg) < 0) {
		goto done;
	}

	if (changeRtpAndRtcpPort(&msg) == 1) {
		obuf->s = update_msg(&msg, (unsigned int *) &obuf->len);
	}

	LM_DBG("SENT:\n\n'%s'\n", obuf->s);

done:
	free_sip_msg(&msg);

	return 0;
}
