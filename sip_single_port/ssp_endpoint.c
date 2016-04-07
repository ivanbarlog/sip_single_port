#include "ssp_endpoint.h"

static char *parse_creator_ip(sip_msg_t *msg) {
    unsigned int a, b, c, d, success;
    char *creator;
    str sdp = {0, 0};

    if (get_msg_body(msg, &sdp) != 0) {
        ERR("Cannot parse SDP.");
        return NULL;
    }

    // todo: this needs to be refactored so also IPv6 is supported (stream has c= information parsed)
    creator = strstr(sdp.s, "c=IN IP4 ");
    sscanf(creator + 9, "%d.%d.%d.%d", &a, &b, &c, &d);

    char *ip = NULL;
    success = asprintf(&ip, "%d.%d.%d.%d", a, b, c, d);

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return ip;
}

void destroy_endpoint(endpoint_t *endpoint) {
    if (endpoint->ip != NULL)
        shm_free(endpoint->ip);

    if (endpoint->streams != NULL)
        destroy_endpoint_streams(&(endpoint->streams));

    // we don't need to free call_id and sibling
    // since they are just pointers and will be freed after whole endpoint is

    shm_free(endpoint);
}

endpoint_t *parse_endpoint(sip_msg_t *msg) {
    endpoint_t *endpoint = (endpoint_t *) shm_malloc(sizeof(endpoint_t));

    if (endpoint == NULL) {
        ERR("cannot allocate pkg memory\n");
        return NULL;
    }

    endpoint->ip = NULL;
    endpoint->sibling = NULL;
    endpoint->streams = NULL;


    if (parse_sdp(msg) != 0) {
        ERR("Cannot parse SDP or body not present, destroying endpoint\n");
        destroy_endpoint(endpoint);

        return NULL;
    }

    char *creator_ip = parse_creator_ip(msg);
    if (creator_ip == NULL) {
        ERR("Cannot parse creator IP (c=), destroying endpoint\n");
        destroy_endpoint(endpoint);

        return NULL;
    }

    shm_copy_string(creator_ip, strlen(creator_ip), &(endpoint->ip));

    if (parse_streams(msg, &endpoint->streams) != 0) {
        ERR("Cannot parse media data, destroying endpoint\n");
        destroy_endpoint(endpoint);

        return NULL;
    }

    return endpoint;
}

static char *get_hdr_line() {
    return " +=========================================+";
}

char *print_endpoint(endpoint_t *endpoint, const char *label) {
    if (endpoint == NULL) {
        return NULL;
    }

    char *result = NULL;
    char *endpoint_info = NULL;
    char *streams_info = NULL;
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
            "%s\n | %-39s |\n%s\n",
            get_hdr_line(),
            endpoint_info,
            streams_info
    );

    if (endpoint_info != NULL)
        free(endpoint_info);

    if (streams_info != NULL)
        free(streams_info);

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}
