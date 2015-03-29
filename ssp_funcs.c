#include "ssp_funcs.h"

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
