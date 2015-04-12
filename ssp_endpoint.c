#include "ssp_endpoint.h"

static int parse_creator_ip(sip_msg_t *msg, char *ip) {
    unsigned int a, b, c, d, success;
    char *creator;
    str sdp = {0, 0};

    sdp.s = get_body(msg);
    if (sdp.s == 0) {
        ERR("Cannot extract body from msg\n");
        return -1;
    }

    sdp.len = msg->len - (sdp.s - msg->buf);

    if (!msg->content_length) {
        ERR("No Content-Length header found\n");
        return -1;
    }

    if (sdp.len != get_content_length(msg)) {
        WARN("Content-Length header value differs from SDP size\n");
    }

    // todo: this needs to be refactored so also IPv6 is supported (stream has c= information parsed)
    creator = strstr(sdp.s, "c=IN IP4 ");
    sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

    success = asprintf(&ip, "%d.%d.%d.%d", a, b, c, d);

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return -1;
    }

    return 0;
}

int parse_endpoint(sip_msg_t *msg, endpoint_t *endpoint) {
    endpoint = NULL;

    endpoint = pkg_malloc(sizeof(endpoint_t));

    if (endpoint == NULL) {
        ERR("cannot allocate pkg memory\n");
        return -1;
    }

    if (parse_sdp(msg) != 0) {
        ERR("Cannot parse SDP or body not present\n");
        return -1;
    }

    parse_streams((sdp_info_t *) msg->body, endpoint->streams);

    parse_creator_ip(msg, endpoint->ip);

    return 0;
}

char *print_endpoint(endpoint_t *endpoint) {
    char *result;
    char *endpoint_info;
    char *streams_info;
    int success;

    success = asprintf(
            &endpoint_info,
            "IP: %s\n",
            endpoint->ip
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    streams_info = print_endpoint_streams(endpoint->streams);

    success = asprintf(
            &result,
            "Endpoint:\n\t%s\n\tStreams:\n\t%s\n\n",
            endpoint_info, streams_info
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}
