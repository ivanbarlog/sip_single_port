#include "ssp_endpoint.h"

/** endpoint structure */
typedef struct endpoint
{
    unsigned short rtp_port;
    unsigned short rtcp_port;
    char ip [50];

    /* usable in UDP socket communication */
    struct sockaddr_in ip_address;

    /* SIP_REQ or SIP_REP */
    unsigned short type;

    struct endpoint *next;

} endpoint;

int parseEndpoint(struct sip_msg *msg, endpoint *endpoint)
{
    if (parse_sdp(msg) == 0)
    {
        str sdp = {0, 0};
        if (get_msg_body(msg, &sdp) == 0)
        {
            str mediamedia;
            str mediaport;
            str mediatransport;
            str mediapayload;
            int is_rtp;

            if (extract_media_attr(&sdp, &mediamedia, &mediaport, &mediatransport, &mediapayload, &is_rtp) == 0)
            {
                char tmp[mediaport.len];
                memcpy(tmp, mediaport.s, mediaport.len);

                endpoint->rtp_port = atoi(tmp);
            }
            else
            {
                LM_ERR("Cannot parse RTP port.\n");

                return -1;
            }

            str rtcp;

            if (extract_rtcp(&sdp, &rtcp) == 0)
            {
                char tmp[rtcp.len];
                memcpy(tmp, rtcp.s, rtcp.len);

                endpoint->rtcp_port = atoi(tmp);
            }
            else
            {
                endpoint->rtcp_port = endpoint->rtp_port + 2;
            }

            unsigned int a, b, c, d;
            char *creator;

            creator = strstr(sdp.s, "c=IN IP4 ");

            sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

            char ip [50];

            int len = sprintf(ip, "%d.%d.%d.%d", a, b, c, d);

            if (len > 0)
            {
                strcpy(endpoint->ip, ip);
            }
            else
            {
                LM_ERR("Cannot parse media IP\n");

                return -1;
            }

            return 0;
        }
        else
        {
            LM_ERR("Cannot get message body\n");

            return -1;
        }
    }
    else
    {
        LM_ERR("Cannot parse SDP.\n");

        return -1;
    }
}


void printEndpoint(endpoint *endpoint)
{
    LM_DBG("Endpoint:\n\tIP: %s\n\tRTP: %d\n\tRTCP: %d\n\tOrigin: %s\n\n",
            endpoint->ip,
            endpoint->rtp_port,
            endpoint->rtcp_port,
            endpoint->type == SIP_REQ ?
                    "SIP_REQUEST" :
                    "SIP_REPLY"
    );
}
