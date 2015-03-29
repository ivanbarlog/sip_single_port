#include "ssp_funcs.h"

int fixSupportedCodecs(struct sip_msg *msg)
{
    //todo
    return 0;
}

char* update_msg(sip_msg_t *msg, unsigned int *olen)
{
    struct dest_info dst;


            LM_DBG("update_msg:\n\n'%s'\n", msg->buf);

    init_dest_info(&dst);
    dst.proto = PROTO_UDP;
    return build_req_buf_from_sip_req(msg,
            olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

int get_msg_type(sip_msg_t *msg)
{
    if (parse_msg(msg->buf, msg->len, msg) == 0)
    {
        //find out if packet is SIP request or reply
        if (msg->first_line.type == SIP_REQUEST)
        {
            return SIP_REQ;
        }
        else if (msg->first_line.type == SIP_REPLY)
        {
            return SIP_REP;
        }
        else
        {
            return -1;
        }
    }
        //if it's not SIP it should be RTP/RTCP
    else
    {
        //second byte from first line
        unsigned char byte = msg->buf[1];

        //we are interested in 6th and 5th bite (reading from right to left counting from index 0)
#define BIT6 0x40
#define BIT5 0x20

        //if 6th and 5th bit is "10" it's RTCP else RTP
        if (!!(byte & BIT6) == 1 && !!(byte & BIT5) == 0)
        {
            return RTCP;
        }
        else
        {
            return RTP;
        }
    }
}

int skip_media_changes(sip_msg_t *msg)
{
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST)
        {
            int method = msg->REQ_METHOD;

            if (method == 1) {
                return 1;
            }
        }
        else if (msg->first_line.type == SIP_REPLY)
        {
            unsigned int code = msg->REPLY_STATUS;
            if (code >= 200 && code < 300) {
                return 1;
            }
        }
    }

    return -1;
}


/** source: http://www.asipto.com/pub/kamailio-devel-guide/#c06get_msg_body */
int get_msg_body(struct sip_msg *msg, str *body)
{
    /* 'msg' is a pointer to a valid struct sip_msg */

    /* get message body
    - after that whole SIP MESSAGE is parsed
    - calls internally parse_headers(msg, HDR_EOH_F, 0)
    */
    body->s = get_body( msg );
    if (body->s==0)
    {
                LM_ERR("cannot extract body from msg\n");
        return -1;
    }

    body->len = msg->len - (body->s - msg->buf);

    /* content-length (if present) must be already parsed */
    if (!msg->content_length)
    {
                LM_ERR("no Content-Length header found!\n");
        return -1;
    }
    if(body->len != get_content_length( msg ))
                LM_WARN("Content length header value different than body size\n");
    return 0;
}

int changeRtpAndRtcpPort(struct sip_msg *msg) {

            LM_DBG("\n\n\nbefore parsing sdp\n\n\n");
//	if (parse_sdp(msg) == 0) {

    str sdp = {0, 0};
    if (get_msg_body(msg, &sdp) != 0)
    {
        goto error;
    }

            LM_DBG("\n\n\nsdp present and parsed\n\n\n");


    struct subst_expr
//				*seRtcp,
            *seMedia;

//			seRtcp = (struct subst_expr *)"a=rtcp:([0-9]{0,5})";
//			seRtcp->replacement = _host_port;

//            seMedia = (struct subst_expr *)pkg_malloc(sizeof(struct subst_expr));
    char *pattern;
//		asprintf(&pattern, "/(^m=[a-zA-Z]*) ([0-9]{0,5})/m=audio %s/g", _host_port.s);
    asprintf(&pattern, "/m=audio ([0-9]{0,5})/m=audio %s/", (char *) _host_port.s);
            LM_DBG("Pattern for replacement: %s\n", pattern);

    str *subst;
    subst = (str *) pkg_malloc(sizeof(str));
    subst->s = pattern;
    subst->len = strlen(pattern);

    seMedia = subst_parser(subst); //"m=([a-zA-Z]*) ([0-9]{0,5})"

            LM_DBG("Pattern for replacement (from *subst_expr): %s\n", seMedia->replacement.s);

    int count = 0;
    str *tmpBody;//, *newBody;
//		char const *oldBody = (char const *) msg->body;
    char const *oldBody = (char const *) sdp.s;

    tmpBody = subst_str(oldBody, msg, seMedia, &count);

            LM_DBG("Found %d matches for %s\nin body:\n%s\n", count, pattern, oldBody);

    if (count > 0) {
        struct lump *anchor;
        char* buf;
        char* value_s;
        int value_len;

        del_nonshm_lump( &(msg->body_lumps) );
//			remove_lump( msg, msg->body_lumps );
        msg->body_lumps = NULL;

        if (del_lump(msg, sdp.s - msg->buf, sdp.len, 0) == 0)
        {
                    LM_ERR("cannot delete existing body");
            return -1;
        }

        anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);

        if (anchor == 0)
        {
                    LM_ERR("failed to get anchor\n");
            return -1;
        }

        if (msg->content_length==0)
        {
            /* need to add Content-Length */
            int len = tmpBody->len;
            value_s=int2str(len, &value_len);

            len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
            buf=pkg_malloc(sizeof(char)*(len));

            if (buf==0)
            {
                        LM_ERR("out of pkg memory\n");
                return -1;
            }

            memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
            memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
            memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
            if (insert_new_lump_after(anchor, buf, len, 0) == 0)
            {
                        LM_ERR("failed to insert content-length lump\n");
                pkg_free(buf);
                return -1;
            }
        }

        anchor = anchor_lump(msg, sdp.s - msg->buf, 0, 0);
        if (anchor == 0)
        {
                    LM_ERR("failed to get body anchor\n");
            return -1;
        }

        buf=pkg_malloc(sizeof(char)*(tmpBody->len));
        if (buf==0)
        {
                    LM_ERR("out of pkg memory\n");
            return -1;
        }
        memcpy(buf, tmpBody->s, tmpBody->len);
        if (insert_new_lump_after(anchor, buf, tmpBody->len, 0) == 0)
        {
                    LM_ERR("failed to insert body lump\n");
            pkg_free(buf);
            pkg_free(tmpBody);
            return -1;
        }
                LM_DBG("new body: [%.*s]", tmpBody->len, tmpBody->s);

        msg->body_lumps = anchor;

                LM_DBG("MSG NEW BUF\n%s\n", msg->buf);

        return 1;
    }
//	}

    error:
            LM_ERR("Cannot change SDP.\n");
    return -1;
}
