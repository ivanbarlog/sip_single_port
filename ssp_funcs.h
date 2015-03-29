#ifndef _KAMAILIO_SSP_FUNCS_H_
#define _KAMAILIO_SSP_FUNCS_H_


#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"

int get_msg_body(struct sip_msg *msg, str *body);

#endif //_KAMAILIO_SSP_FUNCS_H_
