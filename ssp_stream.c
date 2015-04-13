#include "ssp_stream.h"

int parse_streams(sdp_info_t *sdp_info, endpoint_stream_t **streams) {
    endpoint_stream_t *current;
    current = *streams;

    sdp_session_cell_t *sec;
    sec = sdp_info->sessions;
    while (sec != NULL) {
        sdp_stream_cell_t *stc;
        stc = sec->streams;
        while (stc != NULL) {
            endpoint_stream_t *tmp;
            tmp = pkg_malloc(sizeof(endpoint_stream_t));

            if (tmp == NULL) {
                ERR("cannot allocate pkg memory\n");
                return -1;
            }

            LM_DBG(
                    "ZZZ:\n%.*s\n%.*s\n%.*s\n",
                    stc->media.len, stc->media.s,
                    stc->port.len, stc->port.s,
                    stc->rtcp_port.len, stc->rtcp_port.s
            );

            if (copy_str(&(stc->media), &(tmp->media_raw), &(tmp->media)) == -1) {
                ERR("cannot allocate memory.\n");
                return -1;
            }

            if (copy_str(&(stc->port), &(tmp->port_raw), &(tmp->port)) == -1) {
                ERR("cannot allocate memory.\n");
                return -1;
            }

            if (copy_str(&(stc->rtcp_port), &(tmp->rtp_port_raw), &(tmp->rtcp_port)) == -1) {
                ERR("cannot allocate memory.\n");
                return -1;
            }

//            tmp->media = stc->media;
//            tmp->port = stc->port;
//            tmp->rtcp_port = stc->rtcp_port;
            tmp->next = NULL;

            if (current == NULL) {
                current = tmp;
            } else {
                current->next = tmp;
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
            stream->media->len, stream->media->s,
            stream->port->len, stream->port->s,
            stream->rtcp_port->len, stream->rtcp_port->s
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_endpoint_streams(endpoint_stream_t **streams) {

    if (*streams == NULL) {
        ERR("streams list is not initialized\n");
        return NULL;
    }

    LM_DBG("XXX:\nstreams not empty\n");

    char *result;
    int success = 0;

    endpoint_stream_t *current;
    current = *streams;

    result = print_stream(current);

    LM_DBG("XXX:\n%s\n", result);

    while (current->next != NULL) {
        success = asprintf(
                &result,
                "%s%s",
                result, print_stream(current)
        );

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return NULL;
        }

        current = current->next;
    }

    return result;
}

int get_stream_type(endpoint_stream_t *streams, unsigned short port, str **type) {
    char *str_rtp, *str_rtcp;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        success = asprintf(&str_rtp, "%.*s", current->port->len, current->port->s);

        if (success == -1) {
            ERR("asprintf cannot allocate memory\n");
            return -1;
        }

        int rtp_port = atoi(str_rtp);

        success = asprintf(&str_rtcp, "%.*s", current->rtcp_port->len, current->rtcp_port->s);

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
        if (STR_EQ(*(current->media), type)) {
            success = asprintf(&str_port, "%.*s", current->port->len, current->port->s);

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
