#include "ssp_rtcp.h"

#define SR 200 // Sender Report
#define RR 201 // Receiver Report
#define HEADER 8
#define SENDER_INFO_LENGTH 20
#define REPORT_BLOCK_LENGTH 24

int exceeds_limit(char *rtcp, int threshold) {
    int i;
    char *cur;
    int reception_report_count = rtcp[0] & 0x1f;
    unsigned int type = rtcp[1] & 0xFF;

    switch (type) {
        case SR:
            cur = &rtcp[HEADER + SENDER_INFO_LENGTH];
            INFO("Sender report (%d [%02x])\n%02x\n", type, type, cur[0]);
            break;
        case RR:
            cur = &rtcp[HEADER];
            INFO("Receiver report (%d [%02x])\n%02x\n", type, type, cur[0]);
            break;
        default:
            ERR("RTCP is not Sender nor Receiver report (%d [%02x]).\n", type, type);
            return -1;
    }

    for (i = 0; i < reception_report_count; ++i) {
        unsigned int fraction_lost = cur[4] & 0xFF;

        INFO("%d: %02x\n", i, cur[4]);

        if (fraction_lost > threshold) {
            INFO("Found fraction lost in RTCP which exceeds threshold (%d > %d).\n", fraction_lost, threshold);
            return 1;
        }

        cur = &cur[REPORT_BLOCK_LENGTH * i];
    }

    return 0;
}

int notify(char *ip, unsigned short port, socket_list_t *socket_list, struct socket_info *current_socket) {

    socket_item_t *least_used = find_least_used_socket(socket_list);

    char notify[50];
    sprintf(
            notify,
            "%.*s\n%.*s\n%.*s\n%.*s\n",
            current_socket->address_str.len, current_socket->address_str.s,
            current_socket->port_no_str.len, current_socket->port_no_str.s,
            least_used->socket->address_str.len, least_used->socket->address_str.s,
            least_used->socket->port_no_str.len, least_used->socket->port_no_str.s
    );

    char command[150];
    sprintf(
            command,
            "echo \"%s\" | ssh %s \"cat > /usr/local/src/notify\"",
            notify, ip
    );

    INFO("SENDING NOTIFICATION TO %s:%d\n", ip, port);

    INFO("CMD: `%s`\n", command);

    system(command);

    /**
     * 1. find least used socket
     * 2. send notification -> create notify with contents from socket ip:port to socket ip:port
     *
     * 3. on reregister increment new socket and decrement old one
     */

//    INFO("SENDING NOTIFICATION TO %s:%d\n", ip, port);
    return 1;

//    echo -e "5060\n5070\n" | ssh -p1111 192.168.0.31 "cat > /root/notify.dopici"
//    system(execute command);

}
