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

//    if (msg->first_line.type == SIP_REPLY) {
//        return generate_res_buf_from_sip_res(msg, olen, BUILD_NO_VIA1_UPDATE);
//    } else {
        return build_req_buf_from_sip_req(msg,
            olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
//    }
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

int skip_media_changes(struct sip_msg *msg)
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


    /*
     * dest - finalny text
     * p - kopia inputu
     * input - povodny text
     */

    int assigned = 0;

    /* replace */
    dest=res->s;
    p=input;
    for(l=lst; l; l=l->next) {
        struct replacedPort *current;
        current = pkg_malloc(sizeof(struct replacedPort));
//        current->text.s = pkg_malloc(sizeof(char) * l->size);

        char * str;
        memcpy(str, input + l->offset, l->size + 1);
        str[l->size] = '\0';

//        memcpy(current->text.s, input + l->offset, l->size);
//        current->text.s[l->size] = 0;
//        current->text.len = strlen(current->text.s);
//        current->offset = l->offset;
        int result = 0;
        result = sscanf(str, "%*s%hu", &(current->port));

        if (result < 0) {
            sscanf(str, "a=rtcp:%hu", &(current->port));
        }

        LM_DBG("PORT NUMBER: %d in string: %s\n\n", current->port, str);

//        &(current->port)

        if (assigned == 0) {
            LM_DBG("GGG assigned = %d\n", assigned);
            ports = current;
            assigned = 1;
        } else {
            LM_DBG("GGG assigned = %d\n", assigned);
            ports->next = current;
        }

//        LM_DBG("HHH\n\nl->offset: %d, l->size: %d\n", l->offset, l->size);

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

int changeRtpAndRtcpPort(struct sip_msg *msg, str host_port, str host_uri) {

    LM_DBG("\n\n\nbefore parsing sdp\n\n\n");

    str sdp = {0, 0};
    if (get_msg_body(msg, &sdp) != 0)
    {
        goto error;
    }

    LM_DBG("\n\n\nsdp present and parsed\n\n\n");


    struct subst_expr
            *seMedia,
            *seRtcp;

    char *pattern;
    asprintf(&pattern, "/(m=[[:alpha:]]+ *)([[:digit:]]{0,5})/\\1%s/", (char *) host_port.s);

    str *subst;
    subst = (str *) pkg_malloc(sizeof(str));
    subst->s = pattern;
    subst->len = strlen(pattern);

    seMedia = subst_parser(subst);

    asprintf(&pattern, "/(a=rtcp: *)([0-9]{0,5})/\\1%s/", (char *) host_port.s);

    subst->s = pattern;
    subst->len = strlen(pattern);
    seRtcp = subst_parser(subst);

//    LM_DBG("Pattern for replacement (from *subst_expr): %s, %d\n", seMedia->replacement.s, seMedia->replacement.len);

    int count = 0;
    str *newBody, *tmpBody;
    char const *oldBody = (char const *) sdp.s;
    struct replacedPort *rtp_ports, *rtcp_port;
    rtp_ports = pkg_malloc(sizeof(struct replacedPort));
    rtcp_port = pkg_malloc(sizeof(struct replacedPort));

    newBody = ssp_subst_str(oldBody, msg, seMedia, &count, rtp_ports);

    LM_DBG("Found %d matches for %s\nin body:\n%s\n", count, seMedia->replacement.s, oldBody);

    if (count > 0) {
        count = 0;
        oldBody = (char const *) newBody->s;
        tmpBody = ssp_subst_str(oldBody, msg, seRtcp, &count, rtcp_port);

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
//            if (i->port == NULL) {
//                break;
//            }
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
