#include "ssp_replace.h"

/**
 * Removes old body from SIP message and sets
 * the new one provided in argument.
 */
int ssp_set_body(struct sip_msg *msg, str *nb) {
    struct lump *anchor;
    char *buf;
    int len;
    char *value_s;
    int value_len;
    str body = {0, 0};

    body.len = 0;
    body.s = get_body(msg);
    if (body.s == 0) {
        ERR("malformed sip message\n");
        return -1;
    }

    del_nonshm_lump(&(msg->body_lumps));
    msg->body_lumps = NULL;

    if (msg->content_length) {
        body.len = get_content_length(msg);
        if (body.len > 0) {
            if (body.s + body.len > msg->buf + msg->len) {
                ERR("invalid content length: %d\n", body.len);
                return -1;
            }
            if (del_lump(msg, body.s - msg->buf, body.len, 0) == 0) {
                ERR("cannot delete existing body\n");
                return -1;
            }
        }
    }

    anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);

    if (anchor == 0) {
        ERR("failed to get anchor\n");
        return -1;
    }

    if (msg->content_length == 0) {
        /* need to add Content-Length */
        len = nb->len;
        value_s = int2str(len, &value_len);
                LM_DBG("content-length: %d (%s)\n", value_len, value_s);

        len = CONTENT_LENGTH_LEN + value_len + CRLF_LEN;
        buf = pkg_malloc(sizeof(char) * (len));

        if (buf == 0) {
            ERR("out of pkg memory\n");
            return -1;
        }

        memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
        memcpy(buf + CONTENT_LENGTH_LEN, value_s, value_len);
        memcpy(buf + CONTENT_LENGTH_LEN + value_len, CRLF, CRLF_LEN);
        if (insert_new_lump_after(anchor, buf, len, 0) == 0) {
            ERR("failed to insert content-length lump\n");
            pkg_free(buf);
            return -1;
        }
    }

    anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);

    if (anchor == 0) {
        ERR("failed to get body anchor\n");
        return -1;
    }

    buf = pkg_malloc(sizeof(char) * (nb->len));
    if (buf == 0) {
        ERR("out of pkg memory\n");
        return -1;
    }
    memcpy(buf, nb->s, nb->len);
    if (insert_new_lump_after(anchor, buf, nb->len, 0) == 0) {
        ERR("failed to insert body lump\n");
        pkg_free(buf);
        return -1;
    }

    LM_DBG("new body: [%.*s]", nb->len, nb->s);

    return 1;
}

char *update_msg(sip_msg_t *msg, unsigned int *len) {
    struct dest_info dst;

    init_dest_info(&dst);
    dst.proto = PROTO_UDP;

    return build_req_buf_from_sip_req(msg, len, &dst, BUILD_NO_LOCAL_VIA | BUILD_NO_VIA1_UPDATE);
}

static char *get_replace_ip_pattern(struct socket_info *bind_address) {

    char *pattern;

    int success = asprintf(
            &pattern,
            "/(c=)(.*)\r\n/\\1%s %.*s\r\n/g",
            bind_address->address.af == AF_INET6 ? "IN IP6" : "IN IP4",
            bind_address->address_str.len, bind_address->address_str.s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");

        return NULL;
    }

    return pattern;
}

static char *get_replace_rtcp_pattern(struct socket_info *bind_address) {

    char *pattern;

    int success = asprintf(
            &pattern,
            "/(a=rtcp: *)([[:digit:]]{0,5}.*)\r\n/\\1%s\r\n/g",
            (char *) bind_address->port_no_str.s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");

        return NULL;
    }

    return pattern;
}

static char *get_replace_rtp_pattern(struct socket_info *bind_address) {

    char *pattern;
    int success = asprintf(
            &pattern,
            "/(m=[[:alpha:]]+ *)([[:digit:]]{2,5})/\\1%s/g",
            (char *) bind_address->port_no_str.s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");

        return NULL;
    }

    return pattern;
}

static char *get_replace_rtp_and_add_rtcp_pattern(struct socket_info *bind_address) {

    char *pattern;

    // if stream is set to 0 we don't want to change it so we will skip all ports < 10
    int success = asprintf(
            &pattern,
            "/(m=[[:alpha:]]+ *)([[:digit:]]{2,5})(.*\r\n)/\\1%s\\3a=rtcp:%s\r\n/g",
            (char *) bind_address->port_no_str.s,
            (char *) bind_address->port_no_str.s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");

        return NULL;
    }

    return pattern;
}

static struct subst_expr *fill_subst_expr(char *pattern) {

    str subst = {0, 0};

    subst.s = pattern;
    subst.len = (int) strlen(pattern);

    return subst_parser(&subst);
}

int change_media_ports(sip_msg_t *msg, struct socket_info *bind_address) {

    // parse SDP
    str sdp = {0, 0};
    if (get_msg_body(msg, &sdp) != 0) {
        ERR("Cannot parse SDP.\n");
        return -1;
    }

    struct subst_expr
            *seCreator,
            *seMedia,
            *seRtcp;
    char *pattern;

    int count = 0;
    str *newBody, *tmpBody, *cBody;
    char const *oldBody = (char const *) sdp.s;

    // Replace Creator IP
    pattern = get_replace_ip_pattern(bind_address);
    seCreator = fill_subst_expr(pattern);
    free(pattern);

    cBody = subst_str(oldBody, msg, seCreator, &count);

    if (count > 0) {
        count = 0;
        oldBody = (char const *) cBody->s;
    }

    // Replace RTCP ports
    pattern = get_replace_rtcp_pattern(bind_address);
    seRtcp = fill_subst_expr(pattern);
    free(pattern);

    tmpBody = subst_str(oldBody, msg, seRtcp, &count);

    // Replace RTP ports (if RTCP ports was not found we append them to media attributes)
    if (count > 0) {
        pattern = get_replace_rtp_pattern(bind_address);

        count = 0;
        oldBody = (char const *) tmpBody->s;
    } else {
        pattern = get_replace_rtp_and_add_rtcp_pattern(bind_address);
    }

    seMedia = fill_subst_expr(pattern);
    free(pattern);

    newBody = subst_str(oldBody, msg, seMedia, &count);

    ssp_set_body(msg, newBody);

    return 0;
}

int skip_media_changes(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST) {
            if (msg->REQ_METHOD == METHOD_INVITE) {
                return 1;
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
                return 1;
            }
        }
    }

    return -1;
}
