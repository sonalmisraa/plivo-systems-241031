#include "protocol.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int make_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket");
        exit(1);  }
    return fd;
}

static sockaddr_in make_addr(uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    return a;
}

int main() {
    int in_fd = make_socket();
    sockaddr_in in_addr = make_addr(47010);

    if (bind(in_fd, reinterpret_cast<sockaddr*>(&in_addr), sizeof(in_addr)) < 0) {  perror("bind 47010");
        close(in_fd);
        return 1;  }

    int out_fd = make_socket();
    sockaddr_in relay_addr = make_addr(47001);

    uint8_t parity[PAYLOAD_SIZE];
    memset(parity, 0, sizeof(parity));

    uint16_t cur_group = 0xFFFF;

    unsigned char buf[2048];

    while (true) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < 0) {
 if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        if (n != static_cast<ssize_t>(4 + PAYLOAD_SIZE)) continue;

        uint32_t seq;
        memcpy(&seq, buf, sizeof(seq));
        seq = ntohl(seq);

        const uint8_t* payload = buf + 4;
        uint16_t group = static_cast<uint16_t>(seq / FEC_GROUP_SIZE);

        if (group != cur_group) {cur_group = group;
            memset(parity, 0, PAYLOAD_SIZE); }

        Packet dp{};
        dp.seq = htonl(seq);
        dp.group = htons(group);
        dp.type = DATA;
        memcpy(dp.payload, payload, PAYLOAD_SIZE);

        if (sendto(out_fd, &dp, sizeof(dp), 0,reinterpret_cast<sockaddr*>(&relay_addr), sizeof(relay_addr)) < 0) perror("send DATA");

        for (int i = 0; i < PAYLOAD_SIZE; i++) parity[i] ^= payload[i];

        if ((seq % FEC_GROUP_SIZE) == (FEC_GROUP_SIZE - 1)) {
            Packet pp{};
            pp.seq = htonl(group * FEC_GROUP_SIZE);
            pp.group = htons(group);
            pp.type = PARITY;
            memcpy(pp.payload, parity, PAYLOAD_SIZE);

            if (sendto(out_fd, &pp, sizeof(pp), 0, reinterpret_cast<sockaddr*>(&relay_addr), sizeof(relay_addr)) < 0)   perror("send PARITY");
        }
    }

    close(in_fd);
    close(out_fd);
    return 0;
}