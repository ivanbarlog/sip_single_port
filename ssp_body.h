#ifndef _KAMAILIO_SSP_BODY_H_
#define _KAMAILIO_SSP_BODY_H_

#define _GNU_SOURCE
#include <stdio.h>
#include "../../mem/mem.h"
#include "../../re.h"
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../ip_addr.h"
#include "../../msg_translator.h"
#include "../../parser/parse_fline.h"
#include "../../lump_struct.h"
#include "../../data_lump.h"
#include "../../ut.h"
#include "../../config.h"

#include "ssp_funcs.h"

#define SIP_REQ 1
#define SIP_REP 2
#define RTP 3
#define RTCP 4

int fixSupportedCodecs(struct sip_msg *msg);

char* update_msg(sip_msg_t *msg, unsigned int *olen);

int get_msg_type(sip_msg_t *msg);

int skip_media_changes(sip_msg_t *msg);

int changeRtpAndRtcpPort(struct sip_msg *msg, str host_port, str host_uri);

#endif //_KAMAILIO_SSP_BODY_H_
