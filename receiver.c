/* receiver.c — Event-driven XOR FEC recovery
 * Forwards frames immediately upon arrival or recovery.
 */
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD 160
#define MAX_FRAMES 4000

typedef struct {
    int have_data, have_parity, sent;
    unsigned char data[PAYLOAD], parity[PAYLOAD];
} Slot;

static Slot buf[MAX_FRAMES];

static void send_to_player(int fd, struct sockaddr_in *dst, uint32_t seq, const unsigned char *pay) {
    if (seq >= MAX_FRAMES || buf[seq].sent) return;
    buf[seq].sent = 1;
    unsigned char pkt[4 + PAYLOAD];
    pkt[0] = seq >> 24; pkt[1] = seq >> 16; pkt[2] = seq >> 8; pkt[3] = seq;
    memcpy(pkt + 4, pay, PAYLOAD);
    sendto(fd, pkt, sizeof pkt, 0, (struct sockaddr *)dst, sizeof *dst);
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002"); return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(buf, 0, sizeof buf);

    for (;;) {
        unsigned char pkt[2048];
        ssize_t n = recvfrom(in_fd, pkt, sizeof pkt, 0, NULL, NULL);
        if (n < 4 + PAYLOAD) continue;

        uint32_t raw_seq = (uint32_t)pkt[0]<<24 | (uint32_t)pkt[1]<<16 |
                           (uint32_t)pkt[2]<<8  | pkt[3];
        int is_parity = (raw_seq & 0x80000000u) != 0;
        uint32_t seq = raw_seq & 0x7fffffffu;

        if (seq >= MAX_FRAMES) continue;

        if (!is_parity) {
            if (!buf[seq].have_data) {
                memcpy(buf[seq].data, pkt + 4, PAYLOAD);
                buf[seq].have_data = 1;
                send_to_player(out_fd, &player, seq, pkt + 4);

                // Try to recover partner
                uint32_t p_even = (seq % 2 == 0) ? seq : seq - 1;
                uint32_t p_odd = p_even + 1;
                uint32_t partner = (seq % 2 == 0) ? p_odd : p_even;

                if (buf[p_even].have_parity && !buf[partner].have_data) {
                    unsigned char rec[PAYLOAD];
                    for (int i = 0; i < PAYLOAD; i++)
                        rec[i] = buf[p_even].parity[i] ^ buf[seq].data[i];
                    memcpy(buf[partner].data, rec, PAYLOAD);
                    buf[partner].have_data = 1;
                    send_to_player(out_fd, &player, partner, rec);
                }
            }
        } else {
            // seq is the even frame
            uint32_t p_even = seq;
            uint32_t p_odd = seq + 1;
            
            if (!buf[p_even].have_parity) {
                memcpy(buf[p_even].parity, pkt + 4, PAYLOAD);
                buf[p_even].have_parity = 1;

                // Try to recover if we have exactly one of the pair
                if (buf[p_even].have_data && !buf[p_odd].have_data) {
                    unsigned char rec[PAYLOAD];
                    for (int i = 0; i < PAYLOAD; i++)
                        rec[i] = buf[p_even].parity[i] ^ buf[p_even].data[i];
                    memcpy(buf[p_odd].data, rec, PAYLOAD);
                    buf[p_odd].have_data = 1;
                    send_to_player(out_fd, &player, p_odd, rec);
                }
                else if (!buf[p_even].have_data && buf[p_odd].have_data) {
                    unsigned char rec[PAYLOAD];
                    for (int i = 0; i < PAYLOAD; i++)
                        rec[i] = buf[p_even].parity[i] ^ buf[p_odd].data[i];
                    memcpy(buf[p_even].data, rec, PAYLOAD);
                    buf[p_even].have_data = 1;
                    send_to_player(out_fd, &player, p_even, rec);
                }
            }
        }
    }
    return 0;
}
