#include "ssp_endpoint.h"

/**
 * Head of endpoint list
 */
static endpoint_t * head = NULL;

/**
 * Head of connections list
 */
static connection_t * connections = NULL;

int initConnectionList()
{
    if (connections != NULL) {
        return 1;
    }

    connections = pkg_malloc(sizeof(connection_t));
    if (connections == 0) {
        LM_ERR("out of memory");
        return 1;
    }
    connections->prev = NULL;
    connections->next = NULL;
    connections->request_endpoint = NULL;
    connections->response_endpoint = NULL;

    return 0;
}

void pushConnection(connection_t *connection)
{
    initConnectionList();

    connection_t *current;
    current = connections;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = connection;
    current->next->next = NULL;
    current->next->prev = current;
}

int findConnection(const char *call_id, connection_t *connection)
{
    initConnectionList();

    connection_t *c;

    for (c = connections; c; c = c->next) {

        LM_DBG("comparing: '%s' and '%s'", c->call_id, call_id);

        if (strcmp(c->call_id, call_id) == 0) {
            connection = c;
            return 1;
        }
    }

    connection = NULL;

    return -1;
}

int findConnectionBySrcIp(const char *src_ip, connection_t *connection)
{
    initConnectionList();

    connection_t *c;
    c = connections;

    if (c->request_endpoint == NULL || c->response_endpoint == NULL) {
        LM_ERR("Both request and response endpoint must be defined\n");
        return -1;
    }

    while (c->next != NULL) {
        if (strcmp(c->request_endpoint->ip, src_ip) == 1 || strcmp(c->response_endpoint->ip, src_ip) == 1) {
            connection = c;
            return 1;
        }
        c = c->next;
    }

    connection = NULL;

    return -1;
}

connection_t * createConnection(const char *call_id)
{
    connection_t *connection;
    connection = pkg_malloc(sizeof(connection_t));

    strcpy(connection->call_id, call_id);

    connection->next = NULL;
    connection->prev = NULL;
    connection->request_endpoint = NULL;
    connection->response_endpoint = NULL;

    return connection;
}

struct sockaddr_in* getStreamAddress(endpoint_t *endpoint, const char *streamType)
{
    struct sockaddr_in *ip_address;

    unsigned short rtp_port;
    endpoint_stream_t *c;

    int found = -1;
    for (c = endpoint->streams; c; c = c->next) {
        if (strcmp(c->media.s, streamType) == 0) {
            rtp_port = atoi(c->port.s);
            found = 1;
            break;
        }
    }

    if (found == -1) {
        LM_ERR("Cannot find '%s' stream in endpoint structure.\n", streamType);
        return NULL;
    }

    ip_address = pkg_malloc(sizeof(struct sockaddr_in));

    memset((char *) &ip_address, 0, sizeof(struct sockaddr_in));
    ip_address->sin_family = AF_INET;
    ip_address->sin_addr.s_addr = inet_addr(endpoint->ip);
    ip_address->sin_port = htons(rtp_port);

    return ip_address;
}

/**
 * Parses
 * - all RTP streams from SDP,
 * - RTCP port (if not present it is determined) and
 * - creator IP address,
 *
 * All parsed values are persisted in endpoint parameter
 */
int parseEndpoint(struct sip_msg *msg, endpoint_t *endpoint, int msg_type)
{
    endpoint->type = msg_type;

    if (parse_sdp(msg) == 0)
    {
        str sdp = {0, 0};
        if (get_msg_body(msg, &sdp) == 0)
        {
            endpoint_stream_t *head = NULL;
            sdp_info_t *sdp_info = (sdp_info_t*)msg->body;

            sdp_session_cell_t *sec;
            for (sec = sdp_info->sessions; sec; sec = sec->next) {
                sdp_stream_cell_t *stc;
                for (stc = sec->streams; stc; stc = stc->next) {
                    endpoint_stream_t *tmp;
                    tmp = pkg_malloc(sizeof(endpoint_stream_t));

                    tmp->media = stc->media;
                    tmp->port = stc->port;
                    tmp->rtcp_port = stc->rtcp_port;
                    tmp->next = NULL;

                    if (head == 0) {
                        head = tmp;
                    } else {
                        head->next = tmp;
                    }
                }
            }

            endpoint->streams = head;

//            memcpy(&(endpoint->call_id.s), msg->callid->body.s, msg->callid->body.len + 1);
//            endpoint->call_id.s[msg->callid->body.len] = '\0';
//            endpoint->call_id->len = msg->callid->body.len + 1;

//            endpoint->call_id->s = msg->callid->body.s;
//            endpoint->call_id->len = msg->callid->body.len;

            endpoint->call_id = pkg_malloc(sizeof(str));
            *(endpoint->call_id) = msg->callid->body;

            str rtcp;

            if (extract_rtcp(&sdp, &rtcp) == 0) {
                char tmp[rtcp.len];
                memcpy(tmp, rtcp.s, rtcp.len);

                endpoint->rtcp_port = atoi(tmp);
            } else {
                endpoint->rtcp_port = endpoint->rtp_port + 2;
            }

            unsigned int a, b, c, d;
            char *creator;

            creator = strstr(sdp.s, "c=IN IP4 ");

            sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

            char ip [50];
            int len = sprintf(ip, "%d.%d.%d.%d", a, b, c, d);

            if (len > 0) {
                strcpy(endpoint->ip, ip);
            } else {
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
    LM_DBG("Endpoint:\n\tIP: %s\n\tRTCP: %d\n\tOrigin: %s\n\n",
        endpoint->ip,
        endpoint->rtcp_port,
        endpoint->type == SIP_REQ ?
            "SIP_REQUEST" :
            "SIP_REPLY"
    );

    printEndpointStreams(endpoint->streams);
}

void printEndpointStreams(endpoint_stream_t *head)
{
    endpoint_stream_t *i;
    for (i = head; i; i = i->next) {
        LM_DBG(
            "ENDPOINT STREAM:\n###############\nmedia: %.*s\nport: %.*s\nrtcp port: %.*s\n\n",
            i->media.len, i->media.s,
            i->port.len, i->port.s,
            i->rtcp_port.len, i->rtcp_port.s
        );
    }
}

int initEndpointList()
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

    LM_DBG("List of endpoints initialized successfully - %p.", head);
    return 0;
}

void pushEndpoint(endpoint_t *endpoint)
{
    initEndpointList();

    endpoint_t *current;
    current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = endpoint;
    current->next->next = NULL;
    current->next->prev = current;
}

int findEndpoint(const char * ip, endpoint_t *endpoint)
{
    initEndpointList();

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

int removeEndpoint(const char *ip)
{
    initEndpointList();

    endpoint_t *endpoint = NULL;
    if (findEndpoint(ip, endpoint) != 0) {
        LM_DBG("cannot find endpoint with IP '%s'\n", ip);
        return -1;
    }

    if (endpoint->prev == NULL && endpoint->next == NULL) {
        head = NULL;
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
int endpointExists(const char *ip, int type)
{
    initEndpointList();

    endpoint_t *current;
    current = head;
    while (current->next != NULL) {
        if (keyCmp(ip, current->ip) == 0 && current->type == type) {
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

int findStream(endpoint_stream_t *head, endpoint_stream_t *stream, unsigned short port)
{
    endpoint_stream_t *s;
    stream = NULL;

    char* value_s;
    int value_len;
    value_s = int2str(port, &value_len);

    for (s = head; s; s = s->next) {
        char * s_port;
        memcpy(&s_port, s->port.s, s->port.len);
        s_port[s->port.len] = '\0';

        if (strcmp(s_port, value_s)) {
            stream = s;
            return 0;
        }
    }

    return -1;
}

