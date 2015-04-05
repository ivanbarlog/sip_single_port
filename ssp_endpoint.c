#include "ssp_endpoint.h"

int parseEndpoint(struct sip_msg *msg, endpoint_t *endpoint)
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


void printEndpoint(endpoint_t *endpoint)
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

int initList(endpoint_t *head)
{
    if (head != NULL) {
        LM_DBG("list is already initialized.\n");
        return 1;
    }

    head = pkg_malloc(sizeof(endpoint_t));
    if (head == 0) {
        LM_ERR("out of memory");
        return 1;
    }
    head->prev = NULL;
    head->next = NULL;

    return 0;
}

void pushEndpoint(endpoint_t *head, endpoint_t *endpoint)
{
    endpoint_t *current;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = endpoint;
    current->next->next = NULL;
    current->next->prev = current;
}

int findEndpoint(endpoint_t *head, const char * ip, endpoint_t *endpoint)
{
    endpoint_t *current;
    current = head;
    endpoint = NULL;

    while (current->next != NULL) {
        if (keyCmp(ip, current->ip) == 0) {
            endpoint = current;
            return 0;
        }
        current = current->next;
    }

    return -1;
}

int removeEndpoint(endpoint_t *head, const char *ip)
{
    endpoint_t *endpoint = NULL;
    if (findEndpoint(head, ip, endpoint) != 0) {
        LM_DBG("cannot find endpoint with IP '%s'\n", ip);
        return -1;
    }

    if (endpoint->prev == NULL && endpoint->next == NULL) {
        head = NULL;
        initList(head);
    } else {
        // endpoint must point to head since it has no previous endpoint
        if (endpoint->prev == NULL) {
            head = endpoint->next;
            endpoint->next->prev = head;
        } else if (endpoint->next == NULL) {
            endpoint->prev->next = NULL;
        } else {
            endpoint->prev->next = endpoint->next;
            endpoint->next->prev = endpoint->prev;
        }
    }

    pkg_free(endpoint);

    return 0;
}

/**
 * Checks if endpoint is already in list
 * returns 1 if endpoint exists otherwise -1
 */
int endpointExists(endpoint_t *head, const char *ip)
{
    endpoint_t *current = head;
    while (current->next != NULL) {
        if (keyCmp(ip, current->ip) == 0) {
            return 1;
        }

        current = current->next;
    }

    return -1;
}

/**
 * function for comparing key to value
 */
int keyCmp(const char *key, const char *value)
{
    if (strcmp(key, value)) {
        return 0;
    }

    return -1;
}
