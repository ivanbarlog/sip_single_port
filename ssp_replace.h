#ifndef KAMAILIO_SSP_REPLACE_H
#define KAMAILIO_SSP_REPLACE_H

#define _GNU_SOURCE

#include <stdio.h>
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../re.h"
#include "../../msg_translator.h"
#include "../../parser/parse_fline.h"
#include "../../lump_struct.h"
#include "../../data_lump.h"
#include "../../ut.h"
#include "../../config.h"
#include "../../parser/parse_fline.h"
#include "../../parser/parse_cseq.h"

#include "ssp_functions.h"

/**
 * Updates SIP message before it is sent by Kamailio
 */
char *update_msg(sip_msg_t *msg, unsigned int *len);

/**
 * Adds RTCP directive to body (as a body_lump) if not present
 * Port is calculated based on RTP port
 *
 * Returns 0 on success, -1 otherwise
 *
 * todo: should be called within ssp_set_body()
 */
int add_rtcp_field(sip_msg_t *msg, unsigned short rtp_port);

/**
 * Changes all media attributes (RTP ports) and RTCP ports in SDP
 * to host_port which is by default set to 5060 and it's configurable
 * via config file
 *
 * Returns 0 on success, -1 otherwise
 */
int change_media_ports(sip_msg_t *msg, str host_port);

/**
 * Returns 1 if SIP message is INVITE request or ~200 OK response to INVITE,
 * otherwise -1
 */
int skip_media_changes(sip_msg_t *msg);

#endif //KAMAILIO_SSP_REPLACE_H
