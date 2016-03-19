#include "ssp_functions.h"

msg_type get_msg_type(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {
        if (msg->first_line.type == SIP_REQUEST) {
            return SSP_SIP_REQUEST;
        } else if (msg->first_line.type == SIP_REPLY) {
            return SSP_SIP_RESPONSE;
        }
    } else {
        //second byte from first line
        unsigned char byte = msg->buf[1];

        //if 6th and 5th bit is "10" it's RTCP else RTP
        if (!!(byte & BIT6) == 1 && !!(byte & BIT5) == 0) {
            return SSP_RTCP_PACKET;
        } else {
            return SSP_RTP_PACKET;
        }
    }

    return SSP_OTHER;
}


int get_socket_addr(char *endpoint_ip, unsigned short port, struct sockaddr_in **ip) {
    struct sockaddr_in *tmp = pkg_malloc(sizeof(struct sockaddr_in));

    if (tmp == NULL) {
        ERR("cannot allocate pkg memory\n");
        return -1;
    }

    memset((void *) tmp, 0, (size_t) sizeof(*tmp));
    tmp->sin_family = AF_INET;
    tmp->sin_addr.s_addr = inet_addr(endpoint_ip);
    tmp->sin_port = htons(port);

    *ip = tmp;

    return 0;
}

int parse_call_id(sip_msg_t *msg, str *call_id) {
    if (parse_headers(msg, HDR_CALLID_F, 0) != 0) {
        ERR("error parsing CallID header\n");
        return -1;
    }

    if (msg->callid == NULL || msg->callid->body.s == NULL) {
        ERR("NULL call-id header\n");
        return -1;
    }

    *call_id = msg->callid->body;

    return 0;
}

int get_msg_body(struct sip_msg *msg, str *body) {
    body->s = get_body(msg);
    if (body->s == 0) {
        return -1;
    }

    body->len = msg->len - (body->s - msg->buf);

    if (!msg->content_length) {
        return -1;
    }
    if (body->len != get_content_length(msg)) {
        WARN("Content length header value different than body size\n");
    }

    return 0;
}

int str_to_char(str *value, char **new_value) {
    int success;

    success = asprintf(
            new_value,
            "%.*s",
            value->len, value->s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return -1;
    }

    return 0;
}

int copy_str(str *value, char **new_value, str **copy) {

    if (str_to_char(value, new_value) == -1) {
        ERR("cannot allocate memory.\n");
        return -1;
    }

    *copy = (str *) pkg_malloc(sizeof(str));

    if (*copy == NULL) {
        ERR("cannot allocate pkg memory");
        return -1;
    }

    (*copy)->s = *new_value;
    (*copy)->len = strlen(*new_value);

    return 0;
}