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
 * Checks if SIP message cancels dialog
 * eg.
 * if it is SIP request with method CANCEL (when caller cancels call)
 * or SIP response with ~600 code and INVITE method in Cseq (when callee declines call)
 * or SIP response with ~200 code and CANCEL method in Cseq (when request times out and is terminated)
 */
int cancels_dialog(sip_msg_t *msg);

int is_register_request(sip_msg_t *msg);

int is_register_response(sip_msg_t *msg);

#endif //KAMAILIO_SSP_PARSE_H
