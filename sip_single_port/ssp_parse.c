#include "ssp_parse.h"

int initializes_dialog(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST) {

            if (msg->REQ_METHOD == METHOD_INVITE) {
                return 0;
            }

        } else if (msg->first_line.type == SIP_REPLY) {

            if (parse_headers(msg, HDR_CSEQ_F, 0) != 0) {
                return -1;
            }

            if (&msg->cseq == NULL || &msg->cseq->body.s == NULL) {
                return -1;
            }

            unsigned int code = msg->REPLY_STATUS;
            if (code >= 200 && code < 300 && get_cseq(msg)->method_id == METHOD_INVITE) {
                return 0;
            }
        }
    }

    return -1;
}

int terminates_dialog(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REPLY) {

            if (parse_headers(msg, HDR_CSEQ_F, 0) != 0) {
                return -1;
            }

            if (&msg->cseq == NULL || &msg->cseq->body.s == NULL) {
                return -1;
            }

            unsigned int code = msg->REPLY_STATUS;
            if (code >= 200 && code < 300 && get_cseq(msg)->method_id == METHOD_BYE) {
                return 0;
            }
        }
    }

    return -1;
}

int cancels_dialog(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST) {

            if (msg->REQ_METHOD == METHOD_CANCEL) {
                return 0;
            }

        } else if (msg->first_line.type == SIP_REPLY) {

            if (parse_headers(msg, HDR_CSEQ_F, 0) != 0) {
                return -1;
            }

            if (&msg->cseq == NULL || &msg->cseq->body.s == NULL) {
                return -1;
            }

            unsigned int code = msg->REPLY_STATUS;
            if (code >= 300 && code < 700 && get_cseq(msg)->method_id == METHOD_INVITE) {
                return 0;
            }
        }
    }

    return -1;
}

int is_register_request(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST) {

            if (msg->REQ_METHOD == METHOD_REGISTER) {
                return 0;
            }
        }
    }

    return -1;
}

int is_register_response(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REPLY) {

            if (parse_headers(msg, HDR_CSEQ_F, 0) != 0) {
                return -1;
            }

            if (&msg->cseq == NULL || &msg->cseq->body.s == NULL) {
                return -1;
            }

            unsigned int code = msg->REPLY_STATUS;
            if (code >= 200 && code < 300 && get_cseq(msg)->method_id == METHOD_REGISTER) {
                return 0;
            }
        }
    }

    return -1;
}



