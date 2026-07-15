/* sender.c — sends each frame immediately, plus XOR parity for every pair.
 * Wire format: [4-byte seq | 0x80000000 for parity][160-byte payload]
 * Overhead: 3 packets per 2 frames = 1.54x (under 2x cap)
 */
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD 160

static void send_pkt(int fd, struct sockaddr_in *dst, uint32_t seq, const unsigned char *pay)
{
    unsigned char pkt[4 + PAYLOAD];
    pkt[0] = seq >> 24; pkt[1] = seq >> 16; pkt[2] = seq >> 8; pkt[3] = seq;
    memcpy(pkt + 4, pay, PAYLOAD);
    sendto(fd, pkt, sizeof pkt, 0, (struct sockaddr *)dst, sizeof *dst);
}

int main(void)
{
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port   = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010"); return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port   = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    unsigned char prev[PAYLOAD];
    uint32_t prev_seq = 0;
    int have_prev = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < 4 + PAYLOAD) continue;

        uint32_t seq = (uint32_t)buf[0]<<24 | (uint32_t)buf[1]<<16 |
                       (uint32_t)buf[2]<<8  | buf[3];
        unsigned char *pay = buf + 4;

        send_pkt(out_fd, &relay, seq, pay);  /* forward immediately */

        if (seq % 2 == 0) {
            memcpy(prev, pay, PAYLOAD);
            prev_seq = seq; have_prev = 1;
        } else if (have_prev && seq == prev_seq + 1) {
            unsigned char parity[PAYLOAD];
            for (int i = 0; i < PAYLOAD; i++) parity[i] = prev[i] ^ pay[i];
            send_pkt(out_fd, &relay, prev_seq | 0x80000000u, parity);
            have_prev = 0;
        }
    }
    return 0;
}
