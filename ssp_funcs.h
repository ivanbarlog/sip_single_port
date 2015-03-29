#ifndef _KAMAILIO_SSP_FUNCS_H_
#define _KAMAILIO_SSP_FUNCS_H_

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

#include "../../parser/msg_parser.h"

#define SIP_REQ 1
#define SIP_REP 2
#define RTP 3
#define RTCP 4

int fixSupportedCodecs(struct sip_msg *msg);

char* update_msg(sip_msg_t *msg, unsigned int *olen);

int get_msg_type(sip_msg_t *msg);

int skip_media_changes(sip_msg_t *msg);

int get_msg_body(struct sip_msg *msg, str *body);

int changeRtpAndRtcpPort(struct sip_msg *msg);

#endif //_KAMAILIO_SSP_FUNCS_H_
