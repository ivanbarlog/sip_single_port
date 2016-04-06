#include "ssp_endpoint.h"

static int parse_creator_ip(sip_msg_t *msg, char **ip) {
    unsigned int a, b, c, d, success;
    char *creator;
    str sdp = {0, 0};
    if (get_msg_body(msg, &sdp) != 0) {
        ERR("Cannot parse SDP.");
        return -1;
    }

    // todo: this needs to be refactored so also IPv6 is supported (stream has c= information parsed)
    creator = strstr(sdp.s, "c=IN IP4 ");
    sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

    success = asprintf(ip, "%d.%d.%d.%d", a, b, c, d);

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return -1;
    }

    return 0;
}

endpoint_t *parse_endpoint(sip_msg_t *msg) {
    endpoint_t *endpoint;
    endpoint = pkg_malloc(sizeof(endpoint_t));

    if (endpoint == NULL) {
        ERR("cannot allocate pkg memory\n");
        return NULL;
    }

    if (parse_sdp(msg) != 0) {
        ERR("Cannot parse SDP or body not present\n");
        return NULL;
    }

    endpoint->sibling = NULL;
    endpoint->streams = NULL;
    parse_streams(msg, &endpoint->streams);

    parse_creator_ip(msg, &endpoint->ip);

    return endpoint;
}

static char *get_hdr_line() {
    return " +=========================================+";
}

char *print_endpoint(endpoint_t *endpoint, const char *label) {
    if (endpoint == NULL) {
        return "";
    }

    char *result;
    char *endpoint_info;
    char *streams_info;
    int success;

    success = asprintf(
            &endpoint_info,
            "%s (%s)",
            label, endpoint->ip
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    streams_info = print_endpoint_streams(endpoint->streams);

    success = asprintf(
            &result,
            "%s\n | %-39s |\n | %-39s |\n",
            get_hdr_line(),
            endpoint_info,
            streams_info
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}
