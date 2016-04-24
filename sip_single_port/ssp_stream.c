#include "ssp_stream.h"

static void destroy_stream(endpoint_stream_t *stream) {
    if (stream->temporary != NULL)
        shm_free(stream->temporary);

    if (stream->media != NULL)
        shm_free(stream->media);

    if (stream->port != NULL)
        shm_free(stream->port);

    if (stream->rtcp_port != NULL)
        shm_free(stream->rtcp_port);

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

            tmp->temporary = NULL;
            tmp->media = NULL;
            tmp->port = NULL;
            tmp->rtcp_port = NULL;
            tmp->next = NULL;

            tmp->temporary = (int) shm_malloc(sizeof(int));
            if (tmp->temporary == NULL) {
                ERR("cannot allocate shm memory.\n");
                destroy_stream(tmp);

                return -1;
            }
            tmp->temporary = 0;

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

static unsigned int get_rtcp_port(endpoint_stream_t *stream) {
    char tmp;
    unsigned int rtcp_port;
    int result;

    result = sscanf(stream->rtcp_port, "%u %s", &rtcp_port, &tmp);

    if (result == EOF || rtcp_port == 0) {
        rtcp_port = (unsigned int) atoi(stream->port) + 1;
    }

    return rtcp_port;
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

    int rtp_port, rtcp_port;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        rtp_port = (int) atoi(current->port);
        rtcp_port = (int) get_rtcp_port(current);

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
            *port = (unsigned short) atoi(current->port);

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%s'\n", type);
    return -1;
}

int get_stream_type_rtcp(endpoint_stream_t *streams, unsigned short src_port, char **type) {

    int rtcp_port;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        rtcp_port = (int) get_rtcp_port(current);

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

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        if (strcmp(current->media, type) == 0) {

            *port = (unsigned short) get_rtcp_port(current);

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find stream with type '%s'\n", type);
    return -1;
}

int contain_port(endpoint_stream_t *streams, unsigned short *port) {

    int rtp_port, rtcp_port;

    endpoint_stream_t *current;
    current = streams;

    while (current != NULL) {

        rtp_port = (int) atoi(current->port);
        rtcp_port = (int) get_rtcp_port(current);

        if (rtp_port == *port) {
            DBG("Found RTP port.\n");

            return 0;
        }

        if (rtcp_port == *port) {
            DBG("Found RTCP port.\n");

            return 0;
        }

        current = current->next;
    }

    INFO("Cannot find port '%hu' in endpoint streams.\n", *port);
    return -1;
}
