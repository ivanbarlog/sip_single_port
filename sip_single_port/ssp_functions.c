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

int shm_copy_string(char *original_string, int original_length, char **new_string) {
    // allocate shared memory for new string
    *new_string = (char *) shm_malloc(sizeof(char) * (original_length + 1));

    if (*new_string == NULL) {
        ERR("cannot allocate shm memory");
        return -1;
    }

    // copy original string to new string
    memcpy(new_string, &original_string, original_length + 1);
    // end new string with null character
    (*new_string)[original_length] = '\0';

    return 0;
}

int str_to_char(str *value, char **new_value) {

    *new_value = (char *) shm_malloc(sizeof(char) * (value->len + 1));

    if (*new_value == NULL) {
        ERR("cannot allocate shm memory");
        return -1;
    }

    memcpy(new_value, &(value->s), value->len);
    (*new_value)[value->len] = '\0';

    return 0;
}

int copy_str(str *value, char **new_value, str **copy) {

    if (str_to_char(value, new_value) == -1) {
        ERR("cannot allocate memory.\n");
        return -1;
    }

    *copy = (str *) shm_malloc(sizeof(str));

    if (*copy == NULL) {
        ERR("cannot allocate shm memory");
        return -1;
    }

    (*copy)->s = *new_value;
    (*copy)->len = strlen(*new_value);

    return 0;
}

char * print_hex_str(str *str) {
    int i;
    char *buf = NULL;

    for (i = 0; i < str->len; i++) {
        if (buf == NULL) {
            asprintf(&buf, "%02x ", (unsigned int)(str->s[i] & 0xFF));
        } else {
            asprintf(&buf, "%s%02x ", buf, (unsigned int)(str->s[i] & 0xFF));
        }
    }

    return buf;
}

char * print_hex(char *str) {
    int i;
    char *buf = NULL;

    for (i = 0; i < strlen(str); i++) {
        if (buf == NULL) {
            asprintf(&buf, "%02x ", (unsigned int)(str[i] & 0xFF));
        } else {
            asprintf(&buf, "%s%02x ", buf, (unsigned int)(str[i] & 0xFF));
        }
    }

    return buf;
}