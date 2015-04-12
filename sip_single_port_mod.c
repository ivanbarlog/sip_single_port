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

#include "ssp_parse.h"
#include "ssp_replace.h"

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



done:
	free_sip_msg(&msg);

	return 0;
}
