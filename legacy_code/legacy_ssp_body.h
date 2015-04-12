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
#include "../../parser/parse_fline.h"
#include "../../parser/parse_cseq.h"

#include "ssp_funcs.h"

#define SIP_REQ 1
#define SIP_REP 2
#define RTP 3
#define RTCP 4

typedef enum {MEDIA_ATTRIBUTE, RTCP_ATTRIBUTE} replacedType;
typedef enum { AUDIO, VIDEO, TEXT, APPLICATION, MESSAGE, OTHER } mediaType;

typedef struct replacedPort
{
    unsigned short port;
    replacedType type;
    mediaType media;

    struct replacedPort *next;

} replacedPort;

int getMediaType(const char *type);

int fixSupportedCodecs(struct sip_msg *msg);

char* update_msg(struct sip_msg *msg, unsigned int *olen);

int get_msg_type(struct sip_msg *msg);

int skip_media_changes(struct sip_msg *msg);

int changeRtpAndRtcpPort(struct sip_msg *msg, str host_port, str host_uri);

#endif //_KAMAILIO_SSP_BODY_H_
