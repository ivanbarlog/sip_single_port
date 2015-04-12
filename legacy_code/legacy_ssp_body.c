#include "ssp_body.h"

int fixSupportedCodecs(struct sip_msg *msg)
{
    //todo
    return 0;
}

char* update_msg(struct sip_msg *msg, unsigned int *olen)
{
    struct dest_info dst;

    LM_DBG("update_msg:\n\n'%s'\n", msg->buf);

    init_dest_info(&dst);
    dst.proto = PROTO_UDP;

    return build_req_buf_from_sip_req(msg,
        olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

int get_msg_type(struct sip_msg *msg)
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


//todo check if only 200 OK for Invite - DO NOT pass other 200 OK messages
// http://www.in2eps.com/fo-sip/tk-fo-sip-dialog.html
int skip_media_changes(struct sip_msg *msg)
{
    if (parse_msg(msg->buf, msg->len, msg) == 0) {

        if (msg->first_line.type == SIP_REQUEST)
        {
            int method = msg->REQ_METHOD;

            if (method == METHOD_INVITE) {
                LM_DBG("OOO:\nSIP REQUEST > INVITE\n");
                return 1;
            }
        }
        else if (msg->first_line.type == SIP_REPLY) {
            if (parse_headers(msg, HDR_CSEQ_F, 0) != 0) {
                LM_ERR("Cannot parse Cseq\n");
                return -1;
            }

            if (&msg->cseq == NULL || &msg->cseq->body.s == NULL) {
                LM_ERR("cseq not found\n");
                return -1;
            }

//            LM_DBG("ZZZ:\nFound cseq: %.*s\n", ((struct cseq_body*) msg->cseq)->method.len, ((struct cseq_body*) msg->cseq)->method.s);
//            LM_DBG("XXX:\n%d == %d\n", get_cseq(msg)->method_id, METHOD_INVITE);
            unsigned int code = msg->REPLY_STATUS;
            if (code >= 200 && code < 300 && get_cseq(msg)->method_id == METHOD_INVITE) {
                LM_DBG("OOO:\nSIP REPLY > ~200 OK > CSEQ INVITE\n");
                return 1;
            }
        }
    }

    return -1;
}

static int ssp_set_body(struct sip_msg* msg, str *nb)
{
    struct lump *anchor;
    char* buf;
    int len;
    char* value_s;
    int value_len;
    str body = {0,0};

    body.len = 0;
    body.s = get_body(msg);
    if (body.s==0)
    {
        LM_ERR("malformed sip message\n");
        return -1;
    }

    del_nonshm_lump( &(msg->body_lumps) );
    msg->body_lumps = NULL;

    if (msg->content_length)
    {
        body.len = get_content_length( msg );
        if(body.len > 0)
        {
            if(body.s+body.len>msg->buf+msg->len)
            {
                LM_ERR("invalid content length: %d\n", body.len);
                return -1;
            }
            if(del_lump(msg, body.s - msg->buf, body.len, 0) == 0)
            {
                LM_ERR("cannot delete existing body");
                return -1;
            }
        }
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
        len = nb->len;
        value_s=int2str(len, &value_len);
        LM_DBG("content-length: %d (%s)\n", value_len, value_s);

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

    anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);

    if (anchor == 0)
    {
        LM_ERR("failed to get body anchor\n");
        return -1;
    }

    buf=pkg_malloc(sizeof(char)*(nb->len));
    if (buf==0)
    {
        LM_ERR("out of pkg memory\n");
        return -1;
    }
    memcpy(buf, nb->s, nb->len);
    if (insert_new_lump_after(anchor, buf, nb->len, 0) == 0)
    {
        LM_ERR("failed to insert body lump\n");
        pkg_free(buf);
        return -1;
    }
    LM_DBG("new body: [%.*s]", nb->len, nb->s);
    return 1;
}

/* returns the substitution result in a str, input must be 0 term
 *  0 on no match or malloc error
 *  if count is non zero it will be set to the number of matches, or -1
 *   if error
 */
str* ssp_subst_str(
        const char *input,
        struct sip_msg* msg,
        struct subst_expr* se,
        int* count,
        struct replacedPort *ports
)
{
    str* res;
    struct replace_lst *lst;
    struct replace_lst* l;
    int len;
    int size;
    const char* p;
    char* dest;
    const char* end;


    /* compute the len */
    len=strlen(input);
    end=input+len;
    lst=subst_run(se, input, msg, count);
    if (lst==0){
        LM_DBG("no match\n");
        return 0;
    }
    for (l=lst; l; l=l->next)
        len+=(int)(l->rpl.len)-l->size;
    res=pkg_malloc(sizeof(str));
    if (res==0){
                LM_ERR("mem. allocation error\n");
        goto error;
    }
    res->s=pkg_malloc(len+1); /* space for null termination */
    if (res->s==0){
                LM_ERR("mem. allocation error (res->s)\n");
        goto error;
    }
    res->s[len]=0;
    res->len=len;


    int assigned = 0;

    /* replace */
    dest=res->s;
    p=input;
    for(l=lst; l; l=l->next) {
        struct replacedPort *tmp;
        tmp = pkg_malloc(sizeof(struct replacedPort));

        char * str;
        str = pkg_malloc(sizeof(char) * (l->size + 1));
        memcpy(str, input + l->offset, l->size);
        str[l->size] = '\0';

        int result = 0;
        result = sscanf(str, "%*s%hu", &(tmp->port));
        tmp->type = MEDIA_ATTRIBUTE;

        if (result < 0) {
            sscanf(str, "a=rtcp:%hu", &(tmp->port));
            tmp->type = RTCP_ATTRIBUTE;
        }
        tmp->next = NULL;

        if (assigned == 0) {
            LM_DBG("PORT NUMBER: %d in string: %s\n\n", tmp->port, str);
            ports = tmp;
            assigned = 1;
        } else {
            LM_DBG("ANOTHER PORT NUMBER: %d in string: %s\n\n", tmp->port, str);
            ports->next = tmp;
        }

        size = l->offset + input - p;
        memcpy(dest, p, size); /* copy till offset */
        p += size + l->size; /* skip l->size bytes */
        dest += size;
        if (l->rpl.len) {
            memcpy(dest, l->rpl.s, l->rpl.len);
            dest += l->rpl.len;
        }
    }
    memcpy(dest, p, end-p);
    if(lst) replace_lst_free(lst);
    return res;
    error:
    if (lst) replace_lst_free(lst);
    if (res){
        if (res->s) pkg_free(res->s);
        pkg_free(res);
    }
    if (count) *count=-1;
    return 0;
}

int changeRtpAndRtcpPort(struct sip_msg *msg, str host_port, str host_uri)
{
    str sdp = {0, 0};
    if (get_msg_body(msg, &sdp) != 0)
    {
        goto error;
    }

    struct subst_expr
            *seMedia,
            *seRtcp;

    char *pattern;
    // if stream is set to 0 we don't want to change it so we will skip all ports < 10
    // todo: exclude 0 from regex
    asprintf(&pattern, "/(m=[[:alpha:]]+ *)([[:digit:]]{2,5})/\\1%s/g", (char *) host_port.s);

    str *subst;
    subst = (str *) pkg_malloc(sizeof(str));
    subst->s = pattern;
    subst->len = strlen(pattern);

    seMedia = subst_parser(subst);

    asprintf(&pattern, "/(a=rtcp: *)([[:digit:]]{0,5})/\\1%s/g", (char *) host_port.s);

    subst->s = pattern;
    subst->len = strlen(pattern);
    seRtcp = subst_parser(subst);

    int count = 0;
    str *newBody, *tmpBody;
    char const *oldBody = (char const *) sdp.s;
//    struct replacedPort *rtp_ports, *rtcp_port;
//    rtp_ports = pkg_malloc(sizeof(struct replacedPort));
//    rtcp_port = pkg_malloc(sizeof(struct replacedPort));

    //todo: remove ssp_subst_str and use subst_str instead since all neede information
    // are parsed in parseEndpoint
    newBody = subst_str(oldBody, msg, seMedia, &count);
//    newBody = ssp_subst_str(oldBody, msg, seMedia, &count, rtp_ports);

    LM_DBG("Found %d matches for %s\nin body:\n%s\n", count, seMedia->replacement.s, oldBody);

    if (count > 0) {
        count = 0;
        oldBody = (char const *) newBody->s;
        tmpBody = subst_str(oldBody, msg, seRtcp, &count);
//        tmpBody = ssp_subst_str(oldBody, msg, seRtcp, &count, rtcp_port);

        if (count > 0) {
            newBody->s = tmpBody->s;
            newBody->len = tmpBody->len;
        } else {
            /* todo: if RTCP is not present
             * we need to add it and set it to 5060
             * because if it is not present it is automatically
             * determined to next odd port which is 5061
             *
             * also we need to mark this somewhere in endpoint structure
             */
        }

//        int ctr = 0;
//        struct replacedPort *i;
//        for(i = rtp_ports; i; i=i->next) {
//            LM_DBG("%d.: %d\n", ctr, i->port);
//            ctr++;
//        }

        ssp_set_body(msg, newBody);

        return 1;
    }

error:
    LM_ERR("Cannot change SDP.\n");
    return -1;
}

int getMediaType(const char *type) {
    if (strcmp(type, "audio") == 0) {
        return AUDIO;
    } else if (strcmp(type, "video") == 0) {
        return VIDEO;
    } else if (strcmp(type, "text") == 0) {
        return TEXT;
    } else if (strcmp(type, "application") == 0) {
        return APPLICATION;
    } else if (strcmp(type, "message") == 0) {
        return MESSAGE;
    } else {
        return OTHER;
    }
}
