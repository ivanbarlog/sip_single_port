#include "ssp_media_forward.h"

int send_packet_to_endpoint(str *buffer, struct sockaddr_in dst_ip) {

    int sent_bytes = sendto(
            bind_address->socket,
            buffer->s,
            buffer->len,
            0,
            (const struct sockaddr *) &dst_ip,
            sizeof(struct sockaddr_in)
    );

    if (sent_bytes != buffer->len) {
        ERR("failed to send packet\n");

        return -1;
    }

    return 0;
}

void set_src_ip_and_port(char *ip, unsigned short *port, struct receive_info *ri) {

    *port = ri->src_port;
    sprintf(
            ip,
            "%d.%d.%d.%d",
            ri->src_ip.u.addr[0],
            ri->src_ip.u.addr[1],
            ri->src_ip.u.addr[2],
            ri->src_ip.u.addr[3]
    );

}

int find_dst_port(
        int msg_type,
        endpoint_t *src_endpoint,
        endpoint_t *dst_endpoint,
        unsigned short src_port,
        unsigned short *dst_port,
        char **media_type
) {

    switch (msg_type) {
        case SSP_RTP_PACKET:
            if (get_stream_type(src_endpoint->streams, src_port, media_type) == -1) {
                ERR("Cannot find stream with port '%d'\n", src_port);

                return -1;
            }

            if (get_stream_port(dst_endpoint->streams, *media_type, dst_port) == -1) {
                ERR("Cannot find counter part stream with type '%s'\n", *media_type);

                return -1;
            }

            return 0;

        case SSP_RTCP_PACKET:
            if (get_stream_type_rtcp(src_endpoint->streams, src_port, media_type) == -1) {
                ERR("Cannot find stream with port '%d'\n", src_port);

                return -1;
            }

            if (get_stream_rtcp_port(dst_endpoint->streams, *media_type, dst_port) == -1) {
                ERR("Cannot find counter part stream with type '%s'\n", *media_type);

                return -1;
            }

            return 0;

        default:
            ERR("Unknown message type (%d)\n", msg_type);

            return -1;
    }
}

int parse_tagged_msg(const char *msg, char **call_id, char **media_type, int *tag_length) {

    char *tag;
    const char delim[2] = ":";

    int success = asprintf(
            &tag,
            "%s",
            msg
    );

    if (success == -1) {
        ERR("asprintf failed to allocate memory\n");

        return  -1;
    }

    *tag_length = strlen(tag) + 1;

    char *tmp_call_id = strtok(tag, delim);

    if (tmp_call_id == NULL) {
        ERR("Cannot find Call-ID.\n");

        return -1;
    }

    char *tmp_media_type = strtok(NULL, delim);

    if (tmp_media_type == NULL) {
        ERR("Cannot find Media type.\n");

        return -1;
    }

    if (pkg_copy_string(tmp_call_id, strlen(tmp_call_id), call_id) == -1) {
        ERR("Copying string failed.\n");

        return -1;
    };

    if (pkg_copy_string(tmp_media_type, strlen(tmp_media_type), media_type) == -1) {
        ERR("Copying string failed.\n");

        return -1;
    };

    DBG("Parsed\nCall-ID: '%s'\nMedia type: '%s'\n", *call_id, *media_type);
    return 0;
}

int tag_message(str *obuf, char *call_id, char *media_type) {

    char tag[strlen(call_id) + strlen(media_type) + 1];
    sprintf(
            tag,
            "%s:%s",
            call_id,
            media_type
    );
    tag[strlen(call_id) + strlen(media_type) + 1] = '\0';

    // add byte for '\0' to length
    int tag_length = sizeof(char) * strlen(tag);
    int original_length = obuf->len;

    DBG("Message is about to be tagged by: '%s' (%d)\n", tag, tag_length);

    // 1B is for modified RTP/RTCP v3 header
    int modified_msg_length = 1 + tag_length + 1 + original_length;
    char *modified_msg = pkg_malloc(sizeof(char) * modified_msg_length);

    if (modified_msg == NULL) {
        ERR("Cannot allocate pkg memory.\n");

        return -1;
    }

    unsigned char first_byte = obuf->s[0];

    // set first byte to `bin(11xxxxxx)`
    unsigned char changed_first_byte = first_byte | 0xc0;
    memcpy(modified_msg, &changed_first_byte, sizeof(unsigned char));

    memcpy(&(modified_msg[1]), &tag, tag_length + 1);

    // copy rest of the original message after the tag
    memcpy(&(modified_msg[2 + tag_length]), obuf->s, sizeof(char) * original_length);

    obuf->s = modified_msg;
    obuf->len = modified_msg_length;

    return 0;
}

int remove_tag(str *obuf, int tag_length) {

    char *original_msg;
    int original_msg_length = sizeof(char) * (obuf->len - tag_length - 1);

    // create final_msg which has length of original message minus tag_length B
    original_msg = pkg_malloc(original_msg_length);

    if (original_msg == NULL) {
        ERR("Cannot allocate pkg memory.\n");

        return -1;
    }

    memcpy(original_msg, &(obuf->s[1 + tag_length]), original_msg_length);

    obuf->s = original_msg;
    obuf->len = original_msg_length;

    return 0;
}