#include "ssp_rtcp.h"

#define SR 200
#define RR 201
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

int notify(char *ip, unsigned short port, struct socket_info *socket_list, connection_t **connection_list) {

    INFO("SENDING NOTIFICATION TO %s:%d\n", ip, port);

    return 1;
}
