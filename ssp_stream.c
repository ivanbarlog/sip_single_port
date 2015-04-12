#include "ssp_stream.h"

int parse_streams(sdp_info_t *sdp_info, endpoint_stream_t *streams) {
    endpoint_stream_t *current;
    current = streams;

    sdp_session_cell_t *sec;
    sec = sdp_info->sessions;
    while (sec->next != NULL) {
        sdp_stream_cell_t *stc;
        stc = sec->streams;
        while (stc->next != NULL) {
            endpoint_stream_t *tmp;
            tmp = pkg_malloc(sizeof(endpoint_stream_t));

            if (tmp == NULL) {
                ERR("cannot allocate pkg memory\n");
                return -1;
            }

            tmp->media = stc->media;
            tmp->port = stc->port;
            tmp->rtcp_port = stc->rtcp_port;
            tmp->next = NULL;
            tmp->prev = NULL;

            if (current == NULL) {
                current = tmp;
            } else {
                current->next = tmp;
                tmp->prev = current;
                current = current->next;
            }

            stc = stc->next;
        }

        sec = sec->next;
    }

    return 0;
}

char *print_stream(endpoint_stream_t *stream) {
    char *result;
    int success;

    success = asprintf(
            &result,
            "Media: %.*s\nRTP: %.*s\nRTCP: %.*s\n",
            stream->media.len, stream->media.s,
            stream->port.len, stream->port.s,
            stream->rtcp_port.len, stream->rtcp_port.s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_endpoint_streams(endpoint_stream_t *streams) {
    char *result = 0;
    char *placeholder;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current->next != NULL) {
        /**
         * move placeholder to the end of result string
         */
        placeholder = result + strlen(result);

        success = asprintf(
                &placeholder,
                "%s",
                print_stream(current)
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }

        current = current->next;
    }

    return result;
}

int get_stream_type(endpoint_stream_t *streams, unsigned short port, str *type) {
    char *str_rtp, *str_rtcp;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current->next != NULL) {
        success = asprintf(&str_rtp, "%.*s", current->port.len, current->port.s);

        if (success == -1) {
            ERR("asprintf cannot allocate memory\n");
            return -1;
        }

        int rtp_port = atoi(str_rtp);

        success = asprintf(&str_rtcp, "%.*s", current->rtcp_port.len, current->rtcp_port.s);

        if (success == -1) {
            ERR("asprintf cannot allocate memory\n");
            return -1;
        }

        int rtcp_port = atoi(str_rtcp);

        if (rtp_port == port || rtcp_port == port) {
            *type = current->media;

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with port '%d'\n", port);
    return -1;
}

// todo: add function for RTCP as well
int get_stream_port(endpoint_stream_t *streams, str type, unsigned short *port) {
    char * str_port;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current->next != NULL) {
        if (STR_EQ(current->media, type)) {
            success = asprintf(&str_port, "%.*s", current->port.len, current->port.s);

            if (success == -1) {
                ERR("asprintf failed to allocate memory\n");
                return -1;
            }

            *port = (unsigned int)atoi(str_port);

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%.*s'\n", type.len, type.s);
    return -1;
}
