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

int spm_find_dst_port(
        int msg_type,
        endpoint_t *src_endpoint,
        endpoint_t *dst_endpoint,
        unsigned short src_port,
        unsigned short *dst_port
) {

    char *type = NULL;

    switch (msg_type) {
        case SSP_RTP_PACKET:
            if (get_stream_type(src_endpoint->streams, src_port, &type) == -1) {
                ERR("Cannot find stream with port '%d'\n", src_port);

                return -1;
            }

            if (get_stream_port(dst_endpoint->streams, type, dst_port) == -1) {
                ERR("Cannot find counter part stream with type '%s'\n", type);

                return -1;
            }

            return 0;

        case SSP_RTCP_PACKET:
            if (get_stream_type_rtcp(src_endpoint->streams, src_port, &type) == -1) {
                ERR("Cannot find stream with port '%d'\n", src_port);

                return -1;
            }

            if (get_stream_rtcp_port(dst_endpoint->streams, type, dst_port) == -1) {
                ERR("Cannot find counter part stream with type '%s'\n", type);

                return -1;
            }

            return 0;

        default:
            ERR("Unknown message type (%d)\n", msg_type);

            return -1;
    }
}