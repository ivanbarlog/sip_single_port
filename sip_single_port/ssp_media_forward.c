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