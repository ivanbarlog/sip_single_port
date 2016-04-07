#include "ssp_stream.h"

static void destroy_stream(endpoint_stream_t *stream) {
    if (stream->media_raw != NULL)
        shm_free(stream->media_raw);

    if (stream->port_raw != NULL)
        shm_free(stream->port_raw);

    if (stream->rtcp_port_raw != NULL)
        shm_free(stream->rtcp_port_raw);

    // we don't need to free str properties
    // since they are just pointers and will be freed after whole stream is

    if (stream != NULL)
        shm_free(stream);
}

void destroy_endpoint_streams(endpoint_stream_t **streams) {

    endpoint_stream_t *current = *streams;
    endpoint_stream_t *next = NULL;

    while (current->next != NULL) {
        next = current->next;

        destroy_stream(current);

        current = next;
    }
}

int parse_streams(sip_msg_t *msg, endpoint_stream_t **streams) {
    sdp_info_t *sdp_info = (sdp_info_t *) msg->body;
    endpoint_stream_t *head = NULL;
    endpoint_stream_t *current = NULL;

    sdp_session_cell_t *sec;
    sec = sdp_info->sessions;
    while (sec != NULL) {
        sdp_stream_cell_t *stc;
        stc = sec->streams;
        while (stc != NULL) {
            endpoint_stream_t *tmp = (endpoint_stream_t *) shm_malloc(sizeof(endpoint_stream_t));

            if (tmp == NULL) {
                ERR("cannot allocate pkg memory\n");
                return -1;
            }

            tmp->media = NULL;
            tmp->media_raw = NULL;
            tmp->port = NULL;
            tmp->port_raw = NULL;
            tmp->rtcp_port = NULL;
            tmp->rtcp_port_raw = NULL;
            tmp->next = NULL;

            if (copy_str(&(stc->media), &(tmp->media_raw), &(tmp->media)) == -1) {
                ERR("cannot allocate memory.\n");
                destroy_stream(tmp);

                return -1;
            }

            if (copy_str(&(stc->port), &(tmp->port_raw), &(tmp->port)) == -1) {
                ERR("cannot allocate memory.\n");
                destroy_stream(tmp);

                return -1;
            }

            if (copy_str(&(stc->rtcp_port), &(tmp->rtcp_port_raw), &(tmp->rtcp_port)) == -1) {
                ERR("cannot allocate memory.\n");
                destroy_stream(tmp);

                return -1;
            }

            if (head == NULL) {
                head = tmp;
                current = head;
            } else {
                current->next = tmp;
                current = current->next;
            }

            stc = stc->next;
        }

        sec = sec->next;
    }

    *streams = head;

    return 0;
}

static char *get_hdr_line() {
    return " +==============+============+=============+";
}

static char *get_line() {
    return " +--------------+------------+-------------+";
}

char *print_stream(endpoint_stream_t *stream) {
    char *result;
    int success;

    success = asprintf(
            &result,
            " | %-12s | %-10s | %-11s |\n%s\n",
            stream->media->len != 0 ? stream->media_raw : "none",
            stream->port->len != 0 ? stream->port_raw : "none",
            stream->rtcp_port->len != 0 ? stream->rtcp_port_raw : "none",
            get_line()
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    return result;
}

char *print_endpoint_streams(endpoint_stream_t *streams) {

    if (streams == NULL) {
        ERR("streams list is not initialized\n");
        return NULL;
    }

    char *result = 0;
    int success;

    endpoint_stream_t *current;
    current = streams;

    success = asprintf(
            &result,
            "%s\n | %-12s | %-10s | %-11s |\n%s\n%s",
            get_hdr_line(),
            "Media type", "RTP port", "RTCP port",
            get_line(),
            print_stream(current)
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");
        return NULL;
    }

    char *stream_info = NULL;

    while (current->next != NULL) {
        stream_info = print_stream(current->next);

        success = asprintf(
                &result,
                "%s%s",
                result, stream_info
        );

        if (stream_info != NULL)
            free(stream_info);

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

int get_stream_port(endpoint_stream_t *streams, str type, unsigned short *port) {
    char *str_port;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        if (STR_EQ(*(current->media), type)) {
            success = asprintf(&str_port, "%.*s", current->port->len, current->port->s);

            if (success == -1) {
                ERR("asprintf failed to allocate memory\n");
                return -1;
            }

            *port = (unsigned int) atoi(str_port);

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%.*s'\n", type.len, type.s);
    return -1;
}

int get_stream_type_rtcp(endpoint_stream_t *streams, unsigned short src_port, str **type) {
    char *str_port;
    unsigned int rtp_port, rtcp_port;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        success = asprintf(&str_port, "%.*s", current->port->len, current->port->s);

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return -1;
        }

        rtp_port = (unsigned int) atoi(str_port);

        success = asprintf(&str_port, "%.*s", current->rtcp_port->len, current->rtcp_port->s);

        if (success == -1) {
            ERR("asprintf failed to allocate memory\n");
            return -1;
        }

        rtcp_port = (unsigned int) atoi(str_port);

        rtcp_port = rtcp_port == 0 ? rtp_port + 1 : rtcp_port;

        if (rtcp_port == src_port) {
            *type = current->media;

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with port '%d'\n", src_port);
    return -1;
}

int get_stream_rtcp_port(endpoint_stream_t *streams, str type, unsigned short *port) {
    char tmp;
    unsigned int rtcp_port;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        if (STR_EQ(*(current->media), type)) {
            success = sscanf(current->rtcp_port_raw, "%u %s", &rtcp_port, &tmp);

            if (success == EOF || rtcp_port == 0) {
                *port = (unsigned int) atoi(current->port_raw) + 1;
            } else {
                *port = rtcp_port;
            }

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%.*s'\n", type.len, type.s);
    return -1;
}
