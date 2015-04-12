#ifndef KAMAILIO_SSP_PARSE_H
#define KAMAILIO_SSP_PARSE_H

#include "../../parser/msg_parser.h"
#include "../../parser/parse_fline.h"

/**
 * Checks if SIP message initializes dialog
 * eg.
 * if it is SIP request with INVITE method
 * or SIP response with ~200 code and INVITE method in Cseq
 *
 * Returns 0 if msg initializes dialog, -1 otherwise
 */
int initializes_dialog(sip_msg_t *msg);

/**
 * Checks if SIP message terminates dialog
 * eg.
 * if it is SIP response with ~200 code and BYE method in Cseq
 */
int terminates_dialog(sip_msg_t *msg);

/**
 * Parses connection structure with endpoint structures
 * from SIP request or SIP response
 */
int parse_sip_msg(sip_msg_t *msg, int msg_type);

#endif //KAMAILIO_SSP_PARSE_H
