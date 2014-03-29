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
 * \brief SIP-router topoh :: Module interface
 * \ingroup topoh
 * Module: \ref topoh
 */

/*! \defgroup topoh SIP-router :: Topology hiding
 *
 * This module hides the SIP routing headers that show topology details.
 * It it is not affected by the server being transaction stateless or
 * stateful. The script interpreter gets the SIP messages decoded, so all
 * existing functionality is preserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"

#include "../../modules/sanity/api.h"

#include "th_mask.h"
#include "th_msg.h"

MODULE_VERSION


/** module parameters */
str _th_key = { "aL9.n8~Hm]Z", 0 };
str th_cookie_name = {"TH", 0};
str th_cookie_value = {0, 0};
str th_ip = {"10.1.1.10", 0};
str th_uparam_name = {"line", 0};
str th_uparam_prefix = {"sr-", 0};
str th_vparam_name = {"branch", 0};
str th_vparam_prefix = {"z9hG4bKsr-", 0};

str th_callid_prefix = {"!!:", 3};
str th_via_prefix = {0, 0};
str th_uri_prefix = {0, 0};

int th_param_mask_callid = 0;

int th_sanity_checks = 0;
sanity_api_t scb;

int th_msg_received(void *data);
int th_msg_sent(void *data);
int init_sockets();

/** module functions */
static int mod_init(void);

static param_export_t params[]={
	{"mask_key",		STR_PARAM, &_th_key.s},
	{"mask_ip",		STR_PARAM, &th_ip.s},
	{"mask_callid",		INT_PARAM, &th_param_mask_callid},
	{"uparam_name",		STR_PARAM, &th_uparam_name.s},
	{"uparam_prefix",	STR_PARAM, &th_uparam_prefix.s},
	{"vparam_name",		STR_PARAM, &th_vparam_name.s},
	{"vparam_prefix",	STR_PARAM, &th_vparam_prefix.s},
	{"callid_prefix",	STR_PARAM, &th_callid_prefix.s},
	{"sanity_checks",	INT_PARAM, &th_sanity_checks},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"my_topoh",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	params,
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
	init_sockets();

	sr_event_register_cb(SREV_NET_DATA_IN, th_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, th_msg_sent);
#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif
	return 0;
error:
	return -1;
}

/**
 *
 */
int th_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0)
	{
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST)
	{
		if(!IS_SIP(msg))
		{
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type!=SIP_REPLY) {
		LM_DBG("non sip message\n");
		return 1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0)==-1)
	{
		LM_DBG("parsing headers failed");
		return 2;
	}

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg)<0 || msg->to==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg)==NULL)
	{
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	return 0;
}

int handle;

int init_sockets()
{
	handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (handle <= 0)
	{
		LM_ERR("failed to create socket\n");
		return 1;
	}

	LM_DBG("sockets successfully initialized");

	return 0;
}

int send_rtp(str * data, const struct sockaddr * address, unsigned short port)
{
	int nonBlocking = 1;
	if (fcntl(handle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
	{
		LM_ERR("failed to set non-blocking\n");
		return 2;
	}

	int sent_bytes = sendto(handle, data->s, data->len, 0, (const struct sockaddr*) &address, sizeof(struct sockaddr_in));

	if (sent_bytes != data->len)
	{
		LM_ERR("failed to send packet\n");
		return 3;
	}

	LM_DBG("hovno");

	return 0;
}

/**
 *
 */
int th_msg_received(void *data)
{
	sip_msg_t msg;
	str *obuf;
	char *nbuf = NULL;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if ((unsigned char) obuf->s[0] == (unsigned char) 0x80)
	{
		LM_DBG("\nPRIJATA SPRAVA JE RTP PACKET\n\n");

		int i;
		// 2 for hex char + 1 for ':'; last char don't have ':' and this last bit is for '\n'
                char* buf_str = (char*) malloc((3 * sizeof(unsigned char)) * obuf->len);
                char* buf_ptr = buf_str;
                for (i = 0; i < obuf->len; i++)
                {
                        buf_ptr += sprintf(buf_ptr, "%02X", (unsigned char) obuf->s[i]);
			if (i < obuf->len - 1)
			{
				buf_ptr += sprintf(buf_ptr, ":");
			}
                }
                sprintf(buf_ptr,"\n");
                *(buf_ptr + 1) = '\0';

                LM_DBG("\nHEX:\n#####\n%s\n#####\n\n", buf_str);

                pkg_free(buf_ptr);


/*	        struct sockaddr_in address;
		unsigned short port = 30000;

        	memset((char *) &address, 0, sizeof(address));

	        address.sin_family = AF_INET;
	        address.sin_addr.s_addr = inet_addr("192.168.11.130");
        	address.sin_port = htons(port);


	        int nonBlocking = 1;
		if (fcntl(handle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
		{
			LM_DBG("failed to set non-blocking\n");
			goto done;
		}

		int sent_bytes = sendto(handle, obuf->s, obuf->len, 0, (const struct sockaddr*) &address, sizeof(struct sockaddr_in));

		if (sent_bytes != strlen(data))
        	{
                	LM_DBG("failed to send packet\n");
                	goto done;
        	}*/

		// send socket to port 30000
		/*unsigned short port = 30000;

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr("192.168.11.30");
		address.sin_port = htons(port);

		if (send_rtp(obuf, (const struct sockaddr*) &address, port) == 0)
		{
			LM_DBG("\nsocket sent successfully\n");
		}*/
	}
	else
	{
		LM_DBG("\nPRIJATA SPRAVA:\n\nLength: %d\n#####\n%s\n#####\n\n", obuf->len, obuf->s);
	}

done:
	if (nbuf!=NULL)
	{
		pkg_free(nbuf);
	}
	free_sip_msg(&msg);

	return 0;
}

/**
 *
 */
int th_msg_sent(void *data)
{
	LM_DBG("in th_msg_sent");

	sip_msg_t msg;
	str *obuf;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

done:
	free_sip_msg(&msg);
	return 0;
}

