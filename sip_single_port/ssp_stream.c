#include "ssp_stream.h"

static void destroy_stream(endpoint_stream_t *stream) {
    if (stream->media != NULL)
        shm_free(stream->media);

    if (stream->port != NULL)
        shm_free(stream->port);

    if (stream->rtcp_port != NULL)
        shm_free(stream->rtcp_port);

    // we don't need to free str properties
    // since they are just pointers and will be freed after whole stream is

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
            tmp->port = NULL;
            tmp->rtcp_port = NULL;
            tmp->next = NULL;

            if (shm_copy_string(stc->media.s, stc->media.len, &(tmp->media)) == -1) {
                ERR("cannot copy string.\n");
                destroy_stream(tmp);

                return -1;
            }

            if (shm_copy_string(stc->port.s, stc->port.len, &(tmp->port)) == -1) {
                ERR("cannot copy string.\n");
                destroy_stream(tmp);

                return -1;
            }

            if (shm_copy_string(stc->rtcp_port.s, stc->rtcp_port.len, &(tmp->rtcp_port)) == -1) {
                ERR("cannot copy string.\n");
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
            stream->media != NULL ? stream->media : "none",
            stream->port != NULL ? stream->port : "none",
            stream->rtcp_port != NULL ? stream->rtcp_port : "none",
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

int get_stream_type(endpoint_stream_t *streams, unsigned short port, char **type) {

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        int rtp_port = atoi(current->port);
        int rtcp_port = atoi(current->rtcp_port);

        if (rtp_port == port || rtcp_port == port) {
            *type = current->media;

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with port '%d'\n", port);
    return -1;
}

int get_stream_port(endpoint_stream_t *streams, char *type, unsigned short *port) {

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        if (strcmp(current->media, type) == 0) {
            *port = (unsigned int) atoi(current->port);

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%s'\n", type);
    return -1;
}

int get_stream_type_rtcp(endpoint_stream_t *streams, unsigned short src_port, char **type) {
    unsigned int rtp_port, rtcp_port;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        rtp_port = (unsigned int) atoi(current->port);
        rtcp_port = (unsigned int) atoi(current->rtcp_port);

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

int get_stream_rtcp_port(endpoint_stream_t *streams, char *type, unsigned short *port) {
    char tmp;
    unsigned int rtcp_port;
    int success;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {
        if (strcmp(current->media, type) == 0) {
            success = sscanf(current->rtcp_port, "%u %s", &rtcp_port, &tmp);

            if (success == EOF || rtcp_port == 0) {
                *port = (unsigned int) atoi(current->port) + 1;
            } else {
                *port = rtcp_port;
            }

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%s'\n", type);
    return -1;
}
