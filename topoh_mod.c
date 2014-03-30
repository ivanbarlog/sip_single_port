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
 * \brief SIP-router my_topoh :: Module interface
 * \ingroup my_topoh
 * Module: \ref my_topoh
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
#include <errno.h>

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

MODULE_VERSION


#define SIP_REQ 1
#define SIP_REP 2
#define RTP 3
#define RTCP 4


/* socket handle */
int handle;

int get_msg_type(sip_msg_t *msg);
int th_msg_received(void *data);
int th_msg_sent(void *data);

/** module functions */
static int mod_init(void);
int init_sockets();

/** module exports */
struct module_exports exports= {
	"my_topoh",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	0, /* params */
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
	//init_sockets();

	sr_event_register_cb(SREV_NET_DATA_IN, th_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, th_msg_sent);

	#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
	#endif

	return 0;
}

/*
* initialize socket communication
*/
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


int get_msg_type(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg) == 0)
	{
		//find out if packet is SIP request or reply
		if (msg->first_line.type == SIP_REQUEST)
		{
			return SIP_REQ;
		}
		else if (msg->first_line.type == SIP_REPLY)
		{
			return SIP_REP;
		}
		else
		{
			return -1;
		}
	}
	//if it's not SIP it should be RTP/RTCP
	else
	{
		//second byte from first line 
		unsigned char byte = msg->buf[1];

		//we are interested in 6th and 5th bite (reading from right to left counting from index 0)
		#define BIT6 0x40
		#define BIT5 0x20

		//if 6th and 5th bit is "10" it's RTCP else RTP
		if (!!(byte & BIT6) == 1 && !!(byte & BIT5) == 0)
		{
			return RTCP;
		}
		else
		{
			return RTP;
		}
	}
}

/** source: http://www.asipto.com/pub/kamailio-devel-guide/#c06get_msg_body */
int get_msg_body(struct sip_msg *msg, str *body)
{
	/* 'msg' is a pointer to a valid struct sip_msg */

	/* get message body
	- after that whole SIP MESSAGE is parsed
	- calls internally parse_headers(msg, HDR_EOH_F, 0)
	*/
	body->s = get_body( msg );
	if (body->s==0)
	{
		LM_ERR("cannot extract body from msg\n");
		return -1;
	}

	body->len = msg->len - (body->s - msg->buf);

	/* content-length (if present) must be already parsed */
	if (!msg->content_length)
	{
		LM_ERR("no Content-Length header found!\n");
		return -1;
	}
	if(body->len != get_content_length( msg ))
		LM_WARN("Content length header value different than body size\n");
	return 0;
}

/** endpoint structure */
typedef struct endpoint
{
	unsigned short rtp_port;
	unsigned short rtcp_port;
	char *ip_address;
	char ip [50];

	/* SIP_REQ or SIP_REP */
	unsigned short type;

	struct endpoint *next;

} endpoint;

int parseEndpoint(struct sip_msg *msg, endpoint *endpoint)
{
	if (parse_sdp(msg) == 0)
	{
		str sdp = {0, 0};
		if (get_msg_body(msg, &sdp) == 0)
		{
			str mediamedia;
			str mediaport;
			str mediatransport;
			str mediapayload;
			int is_rtp;

//			LM_DBG("sdp: %s", sdp.s);


			if (extract_media_attr(&sdp, &mediamedia, &mediaport, &mediatransport, &mediapayload, &is_rtp) == 0)
			{
				char tmp[mediaport.len];
				memcpy(tmp, mediaport.s, mediaport.len);

				endpoint->rtp_port = atoi(tmp);
			}
			else
			{
				LM_ERR("Cannot parse RTP port.");

				return -1;
			}

			str rtcp;

			if (extract_rtcp(&sdp, &rtcp) == 0)
			{
				char tmp[rtcp.len];
				memcpy(tmp, rtcp.s, rtcp.len);

				endpoint->rtcp_port = atoi(tmp);
			}
			else
			{
				endpoint->rtcp_port = endpoint->rtp_port + 2;
			}

			LM_DBG("sdp: %s", sdp.s);

			unsigned int a, b, c, d;
			char *creator;

			creator = strstr(sdp.s, "c=IN IP4 ");

			sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

			char ip [50];

			LM_DBG("before sprintf");
			int len = sprintf(ip, "%d.%d.%d.%d", a, b, c, d);

			if (len > 0)
			{
				strcpy(endpoint->ip, ip);
			}
			else
			{
				LM_ERR("Cannot parse media IP");

				return -1;
			}

			//ip = endpoint->ip_address;

			//endpoint->ip_address = ip;
			//LM_DBG("after sprintf %s, %s", ip, endpoint->ip_address);


			/*str mediaip;
			int pf;
			char line;

			if (extract_mediaip(&sdp, &mediaip, &pf, &line) == 1)
			{
				char tmp[mediaip.len];
				memcpy(tmp, mediaip.s, mediaip.len);

				endpoint->ip_address = tmp;
			}
			else
			{
				LM_DBG("MEDIA EXTRACT FAILED");
				return -1;
			}*/


			return 0;
		}
		else
		{
			LM_ERR("Cannot get message body");

			return -1;
		}
	}
	else
	{
		LM_ERR("Cannot parse SDP.");

		return -1;
	}
}


void printEndpoint(endpoint *endpoint)
{
	LM_DBG("Endpoint:\n\tIP: %s\n\tRTP: %d\n\tRTCP: %d\n\tOrigin: %s\n\n",
		endpoint->ip,
		endpoint->rtp_port,
		endpoint->rtcp_port,
		endpoint->type == SIP_REQ ?
			"SIP_REQUEST" :
			"SIP_REPLY"
	);
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

	LM_DBG("\n\n########## MESSAGE\n\n%s\n##########\n\n", obuf->s);
	return 0;

	int msg_type = get_msg_type(&msg);

	LM_DBG("\n\n#####\nMessage type: %d\n\n####", msg_type);

	switch (msg_type)
	{
		case SIP_REQ: //no break
		case SIP_REP:
			{
				//parse RTP/RTCP ports
				endpoint ep;

				if (parseEndpoint(&msg, &ep) == 0)
				{
//					LM_DBG("\n\n#####\n\nRTP: %d\nRTCP: %d\n\n#####\n\n", ep.rtp_port, ep.rtcp_port);

					ep.type = msg_type;
					printEndpoint(&ep);
				}
				else
				{
					LM_DBG("failed to parse endpoint");
				}

				break;
			}
		case 3: //no break
		case 4:
			LM_DBG("Message type RTP/RTCP");


		        /*unsigned short local_port = 5060;
		        struct sockaddr_in remote_address;
		        struct sockaddr_in local_address;


		        memset((char *) &remote_address, 0, sizeof(remote_address));
		        remote_address.sin_family = AF_INET;
		        remote_address.sin_addr.s_addr = inet_addr("192.168.11.129");
		        remote_address.sin_port = htons(remote_port);


		        memset((char *) &local_address, 0, sizeof(local_address));
		        local_address.sin_family = AF_INET;
		        local_address.sin_addr.s_addr = inet_addr("192.168.11.1");
		        local_address.sin_port = htons(local_port);


		        if (bind(handle, (const struct sockaddr*) &local_address, sizeof(struct sockaddr_in)) < 0)
		        {
		                printf("failed to bind socket (%s)\n", strerror(errno));
		                return 1;
		        }

		        int nonBlocking = 1;
		        if (fcntl(handle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
		        {
		                printf("failed to set non-blocking\n");
		                return 2;
		        }

		        int sent_bytes = sendto(handle, data, strlen(data), 0, (const struct sockaddr*) &remote_address, sizeof(struct sockaddr_in));

		        if (sent_bytes != strlen(data))
		        {
		      		printf("failed to send packet\n");
		                return 3;
		        }*/

			break;
		default:
			LM_ERR("Unknown message type");
			goto done;
	}



	/*if ((unsigned char) obuf->s[0] == (unsigned char) 0x80)
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

	        struct sockaddr_in address;
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

		if (sent_bytes != obuf->len)
        	{
                	LM_DBG("failed to send packet\n");
                	goto done;
        	}

		// send socket to port 30000
		unsigned short port = 30000;

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr("192.168.11.30");
		address.sin_port = htons(port);

		if (send_rtp(obuf, (const struct sockaddr*) &address, port) == 0)
		{
			LM_DBG("\nsocket sent successfully\n");
		}
	}
	else
	{
		LM_DBG("\nPRIJATA SPRAVA:\n\nLength: %d\n#####\n%s\n#####\n\n", obuf->len, obuf->s);
	}*/

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

	free_sip_msg(&msg);
	return 0;
}

