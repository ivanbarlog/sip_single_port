#include "ssp_functions.h"

msg_type get_msg_type(sip_msg_t *msg) {
    if (parse_msg(msg->buf, msg->len, msg) == 0) {
        if (msg->first_line.type == SIP_REQUEST) {
            return SSP_SIP_REQUEST;
        } else if (msg->first_line.type == SIP_REPLY) {
            return SSP_SIP_RESPONSE;
        }
    } else {
        //second byte from first line
        unsigned char byte = msg->buf[1];

        //if 6th and 5th bit is "10" it's RTCP else RTP
        if (!!(byte & BIT6) == 1 && !!(byte & BIT5) == 0) {
            return SSP_RTCP_PACKET;
        } else {
            return SSP_RTP_PACKET;
        }
    }

    return SSP_OTHER;
}


int get_socket_addr(char *endpoint_ip, unsigned short port, struct sockaddr_in *ip) {
    ip = pkg_malloc(sizeof(struct sockaddr_in));

    if (ip == NULL) {
        ERR("cannot allocate pkg memory\n");
        return -1;
    }

    memset((char *) &ip, 0, sizeof(struct sockaddr_in));
    ip->sin_family = AF_INET;
    ip->sin_addr.s_addr = inet_addr(endpoint_ip);
    ip->sin_port = htons(port);

    return 0;
}
